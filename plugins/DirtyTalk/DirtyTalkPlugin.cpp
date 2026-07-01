/*
 * Dirty Talk - Lo-fi vocal distortion
 * Modes: Vintage Mic / Phone / Megaphone / Small Speaker
 * Copyright (c) 2026 Pilal - ISC license
 *
 * Signal chain (per sample):
 *   1. Noise gate         - peak envelope + smoothed gain (click-free)
 *   2. Band-pass filter   - TPT state-variable filter (input "focus", Freq/Bandwidth)
 *   3. Light compressor    - downward, soft
 *   4. Distortion stage   - per-mode, 2x oversampled (anti-aliased)
 *   5. Device voicing     - per-mode biquad cascade (transducer colouration)
 *   6. DC blocker          - removes offset from asymmetric drive
 *   7. Dry/Wet mix         - smoothed
 */

#include "DistrhoPlugin.hpp"
#include "extra/ScopedDenormalDisable.hpp"

#include <cmath>
#include <algorithm>

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

START_NAMESPACE_DISTRHO

static inline float onePoleCoeff(float seconds, float sampleRate)
{
    if (seconds <= 0.0f)
        return 0.0f;
    return std::exp(-1.0f / (sampleRate * seconds));
}

// --------------------------------------------------------------------------------------------------------------
// Biquad (RBJ cookbook), transposed direct form II.

class Biquad
{
public:
    Biquad() : b0(1.f), b1(0.f), b2(0.f), a1(0.f), a2(0.f), z1(0.f), z2(0.f) {}

    void reset() { z1 = z2 = 0.0f; }

    inline float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass(float fs, float f0, float Q)  { float c,s,al; pre(fs,f0,Q,c,s,al);
        const float a0 = 1.f+al;
        norm((1.f-c)*0.5f, 1.f-c, (1.f-c)*0.5f, a0, -2.f*c, 1.f-al, a0); }

    void setHighpass(float fs, float f0, float Q) { float c,s,al; pre(fs,f0,Q,c,s,al);
        const float a0 = 1.f+al;
        norm((1.f+c)*0.5f, -(1.f+c), (1.f+c)*0.5f, a0, -2.f*c, 1.f-al, a0); }

    void setPeaking(float fs, float f0, float Q, float gainDb) { float c,s,al; pre(fs,f0,Q,c,s,al);
        const float A = std::pow(10.f, gainDb/40.f);
        const float a0 = 1.f + al/A;
        norm(1.f+al*A, -2.f*c, 1.f-al*A, a0, -2.f*c, 1.f-al/A, a0); }

private:
    float b0,b1,b2,a1,a2;
    float z1,z2;

    static void pre(float fs, float f0, float Q, float& cosw, float& sinw, float& alpha)
    {
        if (f0 > fs*0.45f) f0 = fs*0.45f;
        if (f0 < 10.f)     f0 = 10.f;
        const float w0 = 2.f*static_cast<float>(M_PI)*f0/fs;
        cosw = std::cos(w0);
        sinw = std::sin(w0);
        alpha = sinw / (2.f*Q);
    }

    void norm(float nb0,float nb1,float nb2,float a0,float na1,float na2,float /*a0dup*/)
    {
        b0 = nb0/a0; b1 = nb1/a0; b2 = nb2/a0; a1 = na1/a0; a2 = na2/a0;
    }
};

// --------------------------------------------------------------------------------------------------------------
// 2x oversampler with 4th-order Butterworth anti-aliasing (no reported latency, IIR).

class Oversampler2x
{
public:
    void setup(float baseSampleRate)
    {
        const float fs2 = baseSampleRate * 2.0f;
        const float fc  = baseSampleRate * 0.45f; // just below the base Nyquist
        // 4th-order Butterworth = two biquads with these Q values
        up1.setLowpass(fs2, fc, 0.54119610f); up2.setLowpass(fs2, fc, 1.30656296f);
        dn1.setLowpass(fs2, fc, 0.54119610f); dn2.setLowpass(fs2, fc, 1.30656296f);
    }

    void reset() { up1.reset(); up2.reset(); dn1.reset(); dn2.reset(); }

    // Process one base-rate sample through a non-linearity NL at 2x, return base-rate sample.
    template<class NL>
    inline float process(float x, NL&& nl)
    {
        // zero-stuff (x, 0) with x2 gain to compensate, interpolation LPF, non-linearity, decimation LPF
        float a = dn2.process(dn1.process(nl(up2.process(up1.process(x * 2.0f)))));
        float b = dn2.process(dn1.process(nl(up2.process(up1.process(0.0f)))));
        (void)b; // keep the sample aligned with the input (decimate by 2)
        return a;
    }

private:
    Biquad up1, up2, dn1, dn2;
};

// --------------------------------------------------------------------------------------------------------------

static const int kNumModes = 4;
static const int kMaxVoicingSections = 5;

class DirtyTalkPlugin : public Plugin
{
public:
    DirtyTalkPlugin()
        : Plugin(kParameterCount, 0, 0),
          fMode(0.0f), fFreq(1000.0f), fBandwidth(1.0f), fGate(-45.0f), fDryWet(1.0f),
          fDrive(0.0f), fOutput(0.0f),
          fSampleRate(static_cast<float>(getSampleRate())),
          fFreqSmooth(1000.0f), fDryWetSmooth(1.0f),
          fDriveSmooth(1.0f), fOutSmooth(1.0f),
          fG(0.0f), fK(1.0f), fA1(0.0f), fA2(0.0f), fA3(0.0f),
          fIc1(0.0f), fIc2(0.0f),
          fGateEnv(0.0f), fGateGain(0.0f), fCompEnv(0.0f),
          fDcX(0.0f), fDcY(0.0f),
          fCurMode(-1), fVoicingTrim(1.0f)
    {
        updateCoeffs();
        fOversampler.setup(fSampleRate);
        configureVoicing(0);
    }

protected:
   /* ----------------------------------------------------------------------------------------------------------
    * Information */

    const char* getLabel() const override       { return "Dirty Talk"; }
    const char* getDescription() const override { return "Lo-fi vocal distortion: vintage mic, phone line, megaphone or small speaker."; }
    const char* getMaker() const override       { return "Pilal"; }
    const char* getHomePage() const override    { return "https://github.com/pilali/Dirty-Talk"; }
    const char* getLicense() const override     { return "ISC"; }
    uint32_t getVersion() const override        { return d_version(1, 2, 0); }
    int64_t getUniqueId() const override        { return d_cconst('D', 't', 'T', 'k'); }

   /* ----------------------------------------------------------------------------------------------------------
    * Parameters */

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        parameter.hints = kParameterIsAutomatable;

        switch (index)
        {
        case kParamMode:
            parameter.name   = "Mode";
            parameter.symbol = "mode";
            parameter.hints |= kParameterIsInteger;
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 3.0f;
            {
                ParameterEnumerationValue* const values = new ParameterEnumerationValue[kNumModes];
                values[0].value = 0.0f; values[0].label = "Vintage Mic";
                values[1].value = 1.0f; values[1].label = "Phone";
                values[2].value = 2.0f; values[2].label = "Megaphone";
                values[3].value = 3.0f; values[3].label = "Small Speaker";
                parameter.enumValues.count = kNumModes;
                parameter.enumValues.values = values;
                parameter.enumValues.restrictedMode = true;
            }
            break;
        case kParamFreq:
            parameter.name   = "Center Freq";
            parameter.symbol = "center_freq";
            parameter.unit   = "Hz";
            parameter.hints |= kParameterIsLogarithmic;
            parameter.ranges.def = 1000.0f;
            parameter.ranges.min = 300.0f;
            parameter.ranges.max = 3000.0f;
            break;
        case kParamBandwidth:
            parameter.name   = "Bandwidth";
            parameter.symbol = "bandwidth";
            parameter.ranges.def = 1.0f;
            parameter.ranges.min = 0.2f;
            parameter.ranges.max = 4.0f;
            break;
        case kParamGate:
            parameter.name   = "Gate";
            parameter.symbol = "gate";
            parameter.unit   = "dB";
            parameter.ranges.def = -45.0f;
            parameter.ranges.min = -60.0f;
            parameter.ranges.max = 0.0f;
            break;
        case kParamDryWet:
            parameter.name   = "Dry/Wet";
            parameter.symbol = "dry_wet";
            parameter.ranges.def = 1.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            break;
        case kParamDrive:
            parameter.name   = "Drive";
            parameter.symbol = "drive";
            parameter.unit   = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -12.0f;
            parameter.ranges.max = 24.0f;
            break;
        case kParamOutput:
            parameter.name   = "Output";
            parameter.symbol = "output";
            parameter.unit   = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -24.0f;
            parameter.ranges.max = 24.0f;
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index)
        {
        case kParamMode:      return fMode;
        case kParamFreq:      return fFreq;
        case kParamBandwidth: return fBandwidth;
        case kParamGate:      return fGate;
        case kParamDryWet:    return fDryWet;
        case kParamDrive:     return fDrive;
        case kParamOutput:    return fOutput;
        default:              return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParamMode:      fMode = value;      break;
        case kParamFreq:      fFreq = value;      break;
        case kParamBandwidth: fBandwidth = value; break;
        case kParamGate:      fGate = value;      break;
        case kParamDryWet:    fDryWet = value;    break;
        case kParamDrive:     fDrive = value;     break;
        case kParamOutput:    fOutput = value;    break;
        }
    }

   /* ----------------------------------------------------------------------------------------------------------
    * Audio/MIDI Processing */

    void sampleRateChanged(double newSampleRate) override
    {
        fSampleRate = static_cast<float>(newSampleRate);
        updateCoeffs();
        fOversampler.setup(fSampleRate);
        fCurMode = -1; // force voicing rebuild at this sample rate
    }

    void activate() override
    {
        fIc1 = fIc2 = 0.0f;
        fGateEnv = fCompEnv = 0.0f;
        fGateGain = 0.0f;
        fDcX = fDcY = 0.0f;
        fFreqSmooth = fFreq;
        fDryWetSmooth = fDryWet;
        fDriveSmooth = std::pow(10.0f, fDrive / 20.0f);
        fOutSmooth   = std::pow(10.0f, fOutput / 20.0f);
        fOversampler.reset();
        updateCoeffs();
        fCurMode = -1;
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const ScopedDenormalDisable sdd;

        const float* const in  = inputs[0];
        float* const       out = outputs[0];

        const float ctrlCoeff = onePoleCoeff(0.020f, fSampleRate);
        fFreqSmooth = fFreqSmooth * ctrlCoeff + fFreq * (1.0f - ctrlCoeff);
        updateCoeffs();

        const float gateAttack  = onePoleCoeff(0.002f, fSampleRate);
        const float gateRelease = onePoleCoeff(0.150f, fSampleRate);
        const float gateGainCo  = onePoleCoeff(0.005f, fSampleRate);
        const float gateThresh  = std::pow(10.0f, fGate / 20.0f);

        const float compThresh  = std::pow(10.0f, -24.0f / 20.0f);
        const float compRatio   = 2.5f;
        const float compSlope   = 1.0f / compRatio - 1.0f;
        const float compAttack  = onePoleCoeff(0.010f, fSampleRate);
        const float compRelease = onePoleCoeff(0.100f, fSampleRate);

        const float mixCoeff  = onePoleCoeff(0.010f, fSampleRate);
        const float gainCoeff = onePoleCoeff(0.010f, fSampleRate);
        const float driveLin  = std::pow(10.0f, fDrive / 20.0f);
        const float outLin    = std::pow(10.0f, fOutput / 20.0f);

        const int modeIdx = std::max(0, std::min(kNumModes - 1, static_cast<int>(fMode + 0.5f)));
        if (modeIdx != fCurMode)
            configureVoicing(modeIdx);

        for (uint32_t i = 0; i < frames; ++i)
        {
            const float drySample = in[i];
            float x = drySample;

            // 1. NOISE GATE
            const float absX = std::fabs(x);
            if (absX > fGateEnv)
                fGateEnv = gateAttack * fGateEnv + (1.0f - gateAttack) * absX;
            else
                fGateEnv = gateRelease * fGateEnv + (1.0f - gateRelease) * absX;
            const float gateTarget = (fGateEnv >= gateThresh) ? 1.0f : 0.0f;
            fGateGain = fGateGain * gateGainCo + gateTarget * (1.0f - gateGainCo);
            x *= fGateGain;

            // 2. BAND-PASS FILTER (TPT SVF, input focus)
            const float v3 = x - fIc2;
            const float v1 = fA1 * fIc1 + fA2 * v3;
            const float v2 = fIc2 + fA2 * fIc1 + fA3 * v3;
            fIc1 = 2.0f * v1 - fIc1;
            fIc2 = 2.0f * v2 - fIc2;
            x = fK * v1;

            // 3. LIGHT COMPRESSION
            const float absF = std::fabs(x);
            if (absF > fCompEnv)
                fCompEnv = compAttack * fCompEnv + (1.0f - compAttack) * absF;
            else
                fCompEnv = compRelease * fCompEnv + (1.0f - compRelease) * absF;
            if (fCompEnv > compThresh)
            {
                const float overDb = 20.0f * std::log10(fCompEnv / compThresh);
                x *= std::pow(10.0f, compSlope * overDb / 20.0f);
            }

            // 4. DISTORTION (2x oversampled, anti-aliased)
            // User drive pushes the signal into the per-mode waveshaper. At the
            // 0 dB default (fDriveSmooth -> 1.0) the character is unchanged.
            fDriveSmooth = fDriveSmooth * gainCoeff + driveLin * (1.0f - gainCoeff);
            x *= fDriveSmooth;
            const int m = modeIdx;
            x = fOversampler.process(x, [m](float s){ return distortSample(m, s); });

            // 5. DEVICE VOICING (per-mode biquad cascade)
            for (int s = 0; s < fNumSections; ++s)
                x = fVoicing[s].process(x);
            x *= fVoicingTrim;

            // 6. DC BLOCKER
            const float dcY = x - fDcX + 0.9975f * fDcY;
            fDcX = x;
            fDcY = dcY;
            x = dcY;

            // 7. DRY/WET MIX (smoothed) + OUTPUT GAIN (smoothed)
            fDryWetSmooth = fDryWetSmooth * mixCoeff + fDryWet * (1.0f - mixCoeff);
            fOutSmooth    = fOutSmooth * gainCoeff + outLin * (1.0f - gainCoeff);
            out[i] = (drySample * (1.0f - fDryWetSmooth) + x * fDryWetSmooth) * fOutSmooth;
        }
    }

private:
    // raw parameter targets
    float fMode, fFreq, fBandwidth, fGate, fDryWet;
    float fDrive, fOutput;   // dB
    float fSampleRate;

    float fFreqSmooth, fDryWetSmooth;
    float fDriveSmooth, fOutSmooth;   // smoothed linear gains

    // TPT state-variable filter
    float fG, fK, fA1, fA2, fA3;
    float fIc1, fIc2;

    // dynamics
    float fGateEnv, fGateGain, fCompEnv;

    // DC blocker
    float fDcX, fDcY;

    // distortion oversampling + device voicing
    Oversampler2x fOversampler;
    Biquad fVoicing[kMaxVoicingSections];
    int    fNumSections = 0;
    int    fCurMode;
    float  fVoicingTrim;

    // Per-mode distortion character (called inside the oversampler).
    static inline float distortSample(int mode, float x)
    {
        switch (mode)
        {
        case 0: // Vintage Mic - asymmetric soft saturation
        {
            const float d = x * 2.0f;
            const float y = (d > 0.0f) ? std::tanh(d) : std::tanh(d * 0.8f) * 1.2f;
            return y * 0.7f;
        }
        case 1: // Phone - hard clip, thin and gritty
        {
            const float d = x * 4.0f;
            return std::max(-0.35f, std::min(0.35f, d)) * 2.0f;
        }
        case 2: // Megaphone - asymmetric soft clip (bias -> even harmonics)
        {
            const float d = x * 3.5f + 0.1f;
            return (d / (1.0f + std::fabs(d))) * 1.5f;
        }
        default: // 3: Small Speaker - moderate soft clip (cone drive)
        {
            const float d = x * 2.2f;
            return std::tanh(d) * 0.8f;
        }
        }
    }

    // Build the device voicing (transducer colouration) for a given mode.
    void configureVoicing(int mode)
    {
        const float fs = fSampleRate;
        for (int s = 0; s < kMaxVoicingSections; ++s)
            fVoicing[s].reset();

        switch (mode)
        {
        case 0: // Vintage Mic - warm, mid-forward, rolled top
            fVoicing[0].setHighpass(fs, 110.0f, 0.707f);
            fVoicing[1].setPeaking (fs, 280.0f, 0.8f, -2.5f);
            fVoicing[2].setPeaking (fs, 2500.0f, 0.9f, 3.0f);
            fVoicing[3].setLowpass (fs, 7500.0f, 0.707f);
            fNumSections = 4;
            fVoicingTrim = 1.65f;
            break;
        case 1: // Phone - narrow 300-3400 Hz band with mid peaks
            fVoicing[0].setHighpass(fs, 300.0f, 0.8f);
            fVoicing[1].setPeaking (fs, 1000.0f, 1.0f, 4.0f);
            fVoicing[2].setPeaking (fs, 2300.0f, 1.3f, 3.0f);
            fVoicing[3].setLowpass (fs, 3400.0f, 0.8f);
            fNumSections = 4;
            fVoicingTrim = 0.20f;
            break;
        case 2: // Megaphone - horn honk, thin lows, harsh mids
            fVoicing[0].setHighpass(fs, 450.0f, 0.8f);
            fVoicing[1].setPeaking (fs, 700.0f, 1.0f, -5.0f);
            fVoicing[2].setPeaking (fs, 1600.0f, 1.1f, 8.0f);
            fVoicing[3].setPeaking (fs, 2900.0f, 1.2f, 4.0f);
            fVoicing[4].setLowpass (fs, 3600.0f, 0.8f);
            fNumSections = 5;
            fVoicingTrim = 0.60f;
            break;
        default: // 3: Small Speaker - limited lows, presence + breakup peaks
            fVoicing[0].setHighpass(fs, 180.0f, 0.8f);
            fVoicing[1].setPeaking (fs, 3000.0f, 1.0f, 4.0f);
            fVoicing[2].setPeaking (fs, 5500.0f, 1.4f, 3.0f);
            fVoicing[3].setLowpass (fs, 7000.0f, 0.8f);
            fNumSections = 4;
            fVoicingTrim = 1.20f;
            break;
        }
        fCurMode = mode;
    }

    void updateCoeffs()
    {
        float q = fBandwidth;
        if (q < 0.1f) q = 0.1f;

        float fc = fFreqSmooth;
        const float fcMax = fSampleRate * 0.45f;
        if (fc > fcMax) fc = fcMax;
        if (fc < 10.0f) fc = 10.0f;

        fG = std::tan(static_cast<float>(M_PI) * fc / fSampleRate);
        fK = 1.0f / q;
        fA1 = 1.0f / (1.0f + fG * (fG + fK));
        fA2 = fG * fA1;
        fA3 = fG * fA2;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DirtyTalkPlugin)
};

Plugin* createPlugin()
{
    return new DirtyTalkPlugin();
}

END_NAMESPACE_DISTRHO

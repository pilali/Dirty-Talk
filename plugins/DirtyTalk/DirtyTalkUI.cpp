/*
 * Dirty Talk - UI (DGL + NanoVG)
 * Copyright (c) 2026 Pilal - ISC license
 *
 * A sober, flat, vector UI: a mode selector and four knobs.
 */

#include "DistrhoUI.hpp"

#include <cmath>
#include <cstdio>

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;

// --------------------------------------------------------------------------------------------------------------
// Palette

static const Color kColBg     (24, 26, 30);
static const Color kColPanel  (34, 37, 43);
static const Color kColTrack  (52, 56, 64);
static const Color kColAccent (235, 110, 70);   // warm orange
static const Color kColText   (224, 226, 230);
static const Color kColTextDim(132, 138, 148);

// Knob geometry (270 degree sweep, pointing down at center)
static const float kArcStart = 2.356194f;   //  135 deg
static const float kArcEnd   = 7.068583f;   //  405 deg (135 + 270)

// --------------------------------------------------------------------------------------------------------------

struct KnobDesc {
    uint32_t    index;
    const char* label;
    float       min, max, def;
    bool        logarithmic;
    const char* fmt;        // printf format for the value readout
    float       cx, cy;     // centre (set at construction)
};

class DirtyTalkUI : public UI
{
public:
    DirtyTalkUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
          fMode(0),
          fDragKnob(-1),
          fDragStartY(0.0),
          fDragStartVal(0.0f)
    {
       #ifdef DGL_NO_SHARED_RESOURCES
        // Custom font loaded from disk under our own name.
        createFontFromFile("sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        fFontName = "sans";
       #else
        // loadSharedResources() registers DPF's built-in DejaVu Sans under the
        // internal name NANOVG_DEJAVU_SANS_TTF -- NOT "sans". Selecting "sans"
        // here would find no font and NanoVG would silently draw no text at all
        // (the bug this replaces). Reference the actual registered name instead.
        loadSharedResources();
        fFontName = NANOVG_DEJAVU_SANS_TTF;
       #endif

        // six knobs on two rows of three
        const float kx0 = 110.0f, kdx = 130.0f;   // column centres: 110, 240, 370
        const float ky0 = 175.0f, ky1 = 285.0f;   // row centres

        fKnobs[0] = { kParamFreq,      "FREQ",      300.0f, 3000.0f, 1000.0f, true,  "%.0f Hz", kx0 + 0*kdx, ky0 };
        fKnobs[1] = { kParamBandwidth, "BANDWIDTH", 0.2f,   4.0f,    1.0f,    false, "%.2f",    kx0 + 1*kdx, ky0 };
        fKnobs[2] = { kParamGate,      "GATE",      -60.0f, 0.0f,    -45.0f,  false, "%.0f dB", kx0 + 2*kdx, ky0 };
        fKnobs[3] = { kParamDrive,     "DRIVE",     -12.0f, 24.0f,   0.0f,    false, "%.1f dB", kx0 + 0*kdx, ky1 };
        fKnobs[4] = { kParamOutput,    "OUTPUT",    -24.0f, 24.0f,   0.0f,    false, "%.1f dB", kx0 + 1*kdx, ky1 };
        fKnobs[5] = { kParamDryWet,    "DRY/WET",   0.0f,   1.0f,    1.0f,    false, "%.0f %%", kx0 + 2*kdx, ky1 };

        fKnobVal[0] = 1000.0f;
        fKnobVal[1] = 1.0f;
        fKnobVal[2] = -45.0f;
        fKnobVal[3] = 0.0f;
        fKnobVal[4] = 0.0f;
        fKnobVal[5] = 1.0f;

        setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true);
    }

protected:
   /* ----------------------------------------------------------------------------------------------------------
    * DSP/Plugin callbacks */

    void parameterChanged(uint32_t index, float value) override
    {
        if (index == kParamMode)
        {
            fMode = static_cast<int>(value + 0.5f);
            repaint();
            return;
        }
        for (int k = 0; k < kNumKnobs; ++k)
        {
            if (fKnobs[k].index == index)
            {
                fKnobVal[k] = value;
                repaint();
                return;
            }
        }
    }

   /* ----------------------------------------------------------------------------------------------------------
    * Drawing */

    void onNanoDisplay() override
    {
        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());

        // background
        beginPath();
        rect(0.0f, 0.0f, w, h);
        fillColor(kColBg);
        fill();
        closePath();

        // title
        fontFace(fFontName);
        fontSize(26.0f);
        fillColor(kColText);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(24.0f, 20.0f, "DIRTY TALK", nullptr);

        fontSize(11.0f);
        fillColor(kColTextDim);
        text(26.0f, 50.0f, "PILAL  -  LO-FI VOCAL DISTORTION", nullptr);

        drawModeSelector();

        for (int k = 0; k < kNumKnobs; ++k)
            drawKnob(fKnobs[k], fKnobVal[k]);
    }

   /* ----------------------------------------------------------------------------------------------------------
    * Mouse handling */

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;

        if (ev.press)
        {
            // mode selector?
            const int seg = modeSegmentAt(ev.pos.getX(), ev.pos.getY());
            if (seg >= 0)
            {
                setMode(seg);
                return true;
            }

            // knob?
            for (int k = 0; k < kNumKnobs; ++k)
            {
                if (insideKnob(fKnobs[k], ev.pos.getX(), ev.pos.getY()))
                {
                    fDragKnob = k;
                    fDragStartY = ev.pos.getY();
                    fDragStartVal = normFromValue(fKnobs[k], fKnobVal[k]);
                    editParameter(fKnobs[k].index, true);
                    return true;
                }
            }
            return false;
        }

        // release
        if (fDragKnob >= 0)
        {
            editParameter(fKnobs[fDragKnob].index, false);
            fDragKnob = -1;
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fDragKnob < 0)
            return false;

        const double dy = fDragStartY - ev.pos.getY();
        float norm = fDragStartVal + static_cast<float>(dy) / 200.0f; // 200px = full range
        norm = clamp01(norm);

        applyKnob(fDragKnob, norm);
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        for (int k = 0; k < kNumKnobs; ++k)
        {
            if (insideKnob(fKnobs[k], ev.pos.getX(), ev.pos.getY()))
            {
                float norm = normFromValue(fKnobs[k], fKnobVal[k]);
                norm = clamp01(norm + static_cast<float>(ev.delta.getY()) * 0.02f);
                editParameter(fKnobs[k].index, true);
                applyKnob(k, norm);
                editParameter(fKnobs[k].index, false);
                return true;
            }
        }
        return false;
    }

private:
    static constexpr int kNumKnobs = 6;

    const char* fFontName;
    int   fMode;
    KnobDesc fKnobs[kNumKnobs];
    float fKnobVal[kNumKnobs];

    int    fDragKnob;
    double fDragStartY;
    float  fDragStartVal;

    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    static float normFromValue(const KnobDesc& kd, float value)
    {
        if (kd.logarithmic)
            return std::log(value / kd.min) / std::log(kd.max / kd.min);
        return (value - kd.min) / (kd.max - kd.min);
    }

    static float valueFromNorm(const KnobDesc& kd, float norm)
    {
        if (kd.logarithmic)
            return kd.min * std::pow(kd.max / kd.min, norm);
        return kd.min + norm * (kd.max - kd.min);
    }

    void applyKnob(int k, float norm)
    {
        const float value = valueFromNorm(fKnobs[k], norm);
        fKnobVal[k] = value;
        setParameterValue(fKnobs[k].index, value);
        repaint();
    }

    void setMode(int mode)
    {
        if (mode == fMode)
            return;
        fMode = mode;
        editParameter(kParamMode, true);
        setParameterValue(kParamMode, static_cast<float>(mode));
        editParameter(kParamMode, false);
        repaint();
    }

    // ---- mode selector geometry ----
    static constexpr float kModeX = 210.0f;
    static constexpr float kModeY = 22.0f;
    static constexpr float kModeW = 58.0f;
    static constexpr float kModeH = 26.0f;
    static constexpr float kModeGap = 4.0f;

    static constexpr int kNumModes = 4;

    int modeSegmentAt(double mx, double my) const
    {
        if (my < kModeY || my > kModeY + kModeH)
            return -1;
        for (int i = 0; i < kNumModes; ++i)
        {
            const float x = kModeX + i * (kModeW + kModeGap);
            if (mx >= x && mx <= x + kModeW)
                return i;
        }
        return -1;
    }

    void drawModeSelector()
    {
        static const char* const labels[kNumModes] = { "MIC", "PHONE", "MEGA", "SPKR" };
        fontSize(11.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);

        for (int i = 0; i < kNumModes; ++i)
        {
            const float x = kModeX + i * (kModeW + kModeGap);
            const bool active = (i == fMode);

            beginPath();
            roundedRect(x, kModeY, kModeW, kModeH, 5.0f);
            fillColor(active ? kColAccent : kColPanel);
            fill();
            closePath();

            fillColor(active ? kColBg : kColTextDim);
            text(x + kModeW * 0.5f, kModeY + kModeH * 0.5f, labels[i], nullptr);
        }
    }

    // ---- knob ----
    static constexpr float kKnobR = 26.0f;

    bool insideKnob(const KnobDesc& kd, double mx, double my) const
    {
        const double dx = mx - kd.cx;
        const double dy = my - kd.cy;
        return (dx*dx + dy*dy) <= (kKnobR + 6.0) * (kKnobR + 6.0);
    }

    void drawKnob(const KnobDesc& kd, float value)
    {
        const float norm = clamp01(normFromValue(kd, value));
        const float ang = kArcStart + norm * (kArcEnd - kArcStart);

        // track
        beginPath();
        arc(kd.cx, kd.cy, kKnobR, kArcStart, kArcEnd, NanoVG::CW);
        strokeColor(kColTrack);
        strokeWidth(4.0f);
        stroke();
        closePath();

        // value arc
        beginPath();
        arc(kd.cx, kd.cy, kKnobR, kArcStart, ang, NanoVG::CW);
        strokeColor(kColAccent);
        strokeWidth(4.0f);
        stroke();
        closePath();

        // knob body
        beginPath();
        circle(kd.cx, kd.cy, kKnobR - 6.0f);
        fillColor(kColPanel);
        fill();
        closePath();

        // pointer
        const float px = kd.cx + std::cos(ang) * (kKnobR - 8.0f);
        const float py = kd.cy + std::sin(ang) * (kKnobR - 8.0f);
        beginPath();
        moveTo(kd.cx, kd.cy);
        lineTo(px, py);
        strokeColor(kColText);
        strokeWidth(2.0f);
        stroke();
        closePath();

        // value readout
        char buf[32];
        float shown = value;
        if (kd.index == kParamDryWet)
            shown = value * 100.0f;
        std::snprintf(buf, sizeof(buf), kd.fmt, shown);

        fontSize(12.0f);
        fillColor(kColText);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(kd.cx, kd.cy + kKnobR + 16.0f, buf, nullptr);

        // label
        fontSize(10.0f);
        fillColor(kColTextDim);
        text(kd.cx, kd.cy + kKnobR + 32.0f, kd.label, nullptr);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DirtyTalkUI)
};

UI* createUI()
{
    return new DirtyTalkUI();
}

END_NAMESPACE_DISTRHO

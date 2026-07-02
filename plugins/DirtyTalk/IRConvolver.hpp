/*
 * Dirty Talk - Uniform-partitioned FFT convolution (overlap-save)
 * Copyright (c) 2026 Pilal - ISC license
 *
 * Self-contained, dependency-free real-time convolver:
 *   - hand-rolled iterative radix-2 FFT (power-of-two, like the rest of the
 *     project's hand-rolled DSP: biquads, SVF, oversampler);
 *   - uniform-partitioned overlap-save with a frequency-domain delay line, so
 *     the per-block cost is flat regardless of impulse-response length.
 *
 * Latency is exactly one block (blockSize samples); the host is told via
 * Plugin::setLatency so automatic delay compensation lines everything back up.
 */

#ifndef DIRTY_TALK_IR_CONVOLVER_HPP
#define DIRTY_TALK_IR_CONVOLVER_HPP

#include <vector>
#include <complex>
#include <cmath>
#include <cstdint>
#include <algorithm>

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

class IRConvolver
{
public:
    typedef std::complex<float> cfloat;

    IRConvolver() : fB(0), fN(0), fParts(0), fFdlPos(0), fInCount(0), fScratch() {}

    // Allocate everything for an IR of up to maxIrLen samples, partitioned into
    // blocks of blockSize (rounded up to a power of two). This is the only step
    // that allocates; call it off the audio thread (e.g. sampleRateChanged).
    void prepare(int maxIrLen, int blockSize)
    {
        fB = nextPow2(blockSize < 1 ? 1 : blockSize);
        fN = fB * 2;
        fParts = (maxIrLen + fB - 1) / fB;
        if (fParts < 1)
            fParts = 1;

        buildTwiddles();

        fIRspec.assign(fParts, std::vector<cfloat>(fN, cfloat(0.0f, 0.0f)));
        fFDL.assign(fParts, std::vector<cfloat>(fN, cfloat(0.0f, 0.0f)));
        fFdlPos = 0;

        fPrev.assign(fB, 0.0f);
        fInBlock.assign(fB, 0.0f);
        fOutBlock.assign(fB, 0.0f);
        fInCount = 0;

        fTime.assign(fN, cfloat(0.0f, 0.0f));
        fAcc.assign(fN, cfloat(0.0f, 0.0f));
        fScratch.assign(fN, cfloat(0.0f, 0.0f));
    }

    // Swap in a new impulse response. Recomputes the partition spectra in the
    // buffers already allocated by prepare() -- no allocation, so this is safe
    // to call on an IR change. irLen is clamped to the prepared capacity.
    void setIR(const float* ir, int irLen)
    {
        if (fB == 0)
            return;
        for (int p = 0; p < fParts; ++p)
        {
            std::fill(fScratch.begin(), fScratch.end(), cfloat(0.0f, 0.0f));
            for (int n = 0; n < fB; ++n)
            {
                const int idx = p * fB + n;
                if (idx < irLen)
                    fScratch[n] = cfloat(ir[idx], 0.0f);
            }
            fft(fScratch.data(), false);
            fIRspec[p] = fScratch;
        }
    }

    void reset()
    {
        if (fB == 0)
            return;
        for (auto& v : fFDL)
            std::fill(v.begin(), v.end(), cfloat(0.0f, 0.0f));
        std::fill(fPrev.begin(), fPrev.end(), 0.0f);
        std::fill(fInBlock.begin(), fInBlock.end(), 0.0f);
        std::fill(fOutBlock.begin(), fOutBlock.end(), 0.0f);
        fInCount = 0;
        fFdlPos = 0;
    }

    int latency() const { return fB; }
    bool ready()  const { return fB > 0; }

    // Process one sample. Output lags the input by `latency()` samples.
    inline float processSample(float x)
    {
        const float y = fOutBlock[fInCount];
        fInBlock[fInCount] = x;
        if (++fInCount == fB)
        {
            processBlock();
            fInCount = 0;
        }
        return y;
    }

private:
    int fB, fN, fParts, fFdlPos, fInCount;

    std::vector<std::vector<cfloat>> fIRspec;   // [part][N]
    std::vector<std::vector<cfloat>> fFDL;       // [part][N] circular
    std::vector<float>  fPrev, fInBlock, fOutBlock;
    std::vector<cfloat> fTime, fAcc, fScratch;
    std::vector<cfloat> fTwiddle;                // N/2 forward twiddles
    std::vector<int>    fBitRev;                 // N-length bit-reversal table

    static int nextPow2(int v)
    {
        int p = 1;
        while (p < v) p <<= 1;
        return p;
    }

    void buildTwiddles()
    {
        fTwiddle.resize(fN / 2);
        for (int k = 0; k < fN / 2; ++k)
        {
            const float a = -2.0f * static_cast<float>(M_PI) * k / fN;
            fTwiddle[k] = cfloat(std::cos(a), std::sin(a));
        }
        fBitRev.resize(fN);
        int logn = 0;
        while ((1 << logn) < fN) ++logn;
        for (int i = 0; i < fN; ++i)
        {
            int r = 0;
            for (int b = 0; b < logn; ++b)
                if (i & (1 << b)) r |= 1 << (logn - 1 - b);
            fBitRev[i] = r;
        }
    }

    // In-place iterative radix-2 FFT over fN points. inverse => conjugate
    // twiddles and 1/N scaling.
    void fft(cfloat* a, bool inverse) const
    {
        const int n = fN;
        for (int i = 0; i < n; ++i)
        {
            const int r = fBitRev[i];
            if (r > i) std::swap(a[i], a[r]);
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1;
            const int step = n / len;
            for (int i = 0; i < n; i += len)
            {
                int k = 0;
                for (int j = 0; j < half; ++j, k += step)
                {
                    cfloat w = fTwiddle[k];
                    if (inverse) w = std::conj(w);
                    const cfloat u = a[i + j];
                    const cfloat v = a[i + j + half] * w;
                    a[i + j]        = u + v;
                    a[i + j + half] = u - v;
                }
            }
        }
        if (inverse)
        {
            const float inv = 1.0f / n;
            for (int i = 0; i < n; ++i)
                a[i] *= inv;
        }
    }

    void processBlock()
    {
        // Time buffer = [previous block | current block], length N.
        for (int n = 0; n < fB; ++n)
        {
            fTime[n]      = cfloat(fPrev[n], 0.0f);
            fTime[fB + n] = cfloat(fInBlock[n], 0.0f);
        }
        fft(fTime.data(), false);

        // Store this input spectrum in the FDL.
        fFDL[fFdlPos] = fTime;

        // Y = sum_j IRspec[j] * X[m-j]  (frequency-domain MAC)
        std::fill(fAcc.begin(), fAcc.end(), cfloat(0.0f, 0.0f));
        for (int j = 0; j < fParts; ++j)
        {
            int idx = fFdlPos - j;
            if (idx < 0) idx += fParts;
            const cfloat* X = fFDL[idx].data();
            const cfloat* H = fIRspec[j].data();
            for (int k = 0; k < fN; ++k)
                fAcc[k] += X[k] * H[k];
        }

        fft(fAcc.data(), true);

        // Overlap-save: keep the last B samples (valid linear-convolution part).
        for (int n = 0; n < fB; ++n)
            fOutBlock[n] = fAcc[fB + n].real();

        // Advance state.
        fPrev = fInBlock;
        if (++fFdlPos == fParts)
            fFdlPos = 0;
    }
};

#endif // DIRTY_TALK_IR_CONVOLVER_HPP

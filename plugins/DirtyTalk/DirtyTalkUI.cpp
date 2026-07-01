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
// Palette - 1960s transistor-radio look (cream body, brass grille, red needle)

static const Color kColBg      (215, 199, 162);  // cream body
static const Color kColBgTop   (232, 220, 190);  // lighter top of body
static const Color kColPanel   (239, 230, 207);  // cream pill (top)
static const Color kColPanelLo (214, 196, 156);  // cream pill (bottom)
static const Color kColTrack   (110,  83,  32);  // dark brass knob track
static const Color kColAccent  (210,  70,  50);  // red tuning needle
static const Color kColText    ( 70,  53,  31);  // dark brown
static const Color kColTextDim (138, 109,  62);  // medium brown
static const Color kColGold    (176, 144,  79);  // gold trim / borders
static const Color kColGrille  (169, 132,  60);  // brass grille base
static const Color kColGrilleDk( 90,  66,  22);  // grille perforation / frame
static const Color kColKnobHi  (233, 207, 143);  // brass highlight
static const Color kColKnobLo  (124,  94,  34);  // brass shadow
static const Color kColKnobPtr ( 58,  37,  19);  // knob pointer
static const Color kColGrilleTx(247, 238, 216);  // cream text over the grille

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
          fCab(0),
          fCabIR(0),
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
        if (index == kParamCab)
        {
            fCab = static_cast<int>(value + 0.5f);
            repaint();
            return;
        }
        if (index == kParamCabIR)
        {
            fCabIR = static_cast<int>(value + 0.5f);
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

        // cream body with a soft top-lit gradient
        beginPath();
        rect(0.0f, 0.0f, w, h);
        fillPaint(linearGradient(0.0f, 0.0f, 0.0f, h, kColBgTop, kColBg));
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

        drawGrille();
        for (int k = 0; k < kNumKnobs; ++k)
            drawKnob(fKnobs[k], fKnobVal[k]);

        drawCabBar();

        // gold maker script, bottom-right
        fontSize(15.0f);
        fillColor(kColGold);
        textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
        text(w - 18.0f, h - 12.0f, "Pilal", nullptr);
    }

    // Brass perforated speaker grille behind the two knob rows.
    void drawGrille()
    {
        const float gx = 42.0f, gy = 146.0f, gw = 396.0f, gh = 202.0f, gr = 10.0f;

        // brass panel with a diagonal sheen
        beginPath();
        roundedRect(gx, gy, gw, gh, gr);
        fillPaint(linearGradient(gx, gy, gx + gw * 0.4f, gy + gh, kColKnobHi, kColGrille));
        fill();
        closePath();

        // perforations: a dot grid clipped to the panel
        scissor(gx + 3.0f, gy + 3.0f, gw - 6.0f, gh - 6.0f);
        fillColor(kColGrilleDk);
        for (float yy = gy + 10.0f; yy < gy + gh - 6.0f; yy += 13.0f)
            for (float xx = gx + 10.0f; xx < gx + gw - 6.0f; xx += 13.0f)
            {
                beginPath();
                circle(xx, yy, 1.4f);
                fill();
                closePath();
            }
        resetScissor();

        // recessed brass frame
        beginPath();
        roundedRect(gx, gy, gw, gh, gr);
        strokeColor(kColGrilleDk);
        strokeWidth(2.5f);
        stroke();
        closePath();
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

            // cabinet bar?
            if (handleCabPress(ev.pos.getX(), ev.pos.getY()))
                return true;

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
        // scroll over the cabinet bar cycles the IR
        if (ev.pos.getY() >= kCabY && ev.pos.getY() <= kCabY + kCabH &&
            ev.pos.getX() >= kIRPrevX && ev.pos.getX() <= kIRNextX + kIRBtnW)
        {
            stepIR(ev.delta.getY() > 0.0 ? +1 : -1);
            return true;
        }

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
    // Keep in sync with dirtytalk_irs::kNumIRs in the DSP (the big IR header is
    // not pulled into the UI translation unit just for this count).
    static constexpr int kNumIRs = 20;

    const char* fFontName;
    int   fMode;
    int   fCab;      // cabinet on/off
    int   fCabIR;    // selected IR index
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
            if (active)
                fillColor(kColAccent);
            else
                fillPaint(linearGradient(x, kModeY, x, kModeY + kModeH, kColPanel, kColPanelLo));
            fill();
            strokeColor(kColGold);
            strokeWidth(1.5f);
            stroke();
            closePath();

            fillColor(active ? kColGrilleTx : kColText);
            text(x + kModeW * 0.5f, kModeY + kModeH * 0.5f, labels[i], nullptr);
        }
    }

    // ---- cabinet bar (toggle + IR stepper) ----
    static constexpr float kCabY    = 360.0f;
    static constexpr float kCabH    = 30.0f;
    static constexpr float kCabTogX = 40.0f;
    static constexpr float kCabTogW = 96.0f;
    static constexpr float kIRBtnW  = 30.0f;
    static constexpr float kIRPrevX = 156.0f;
    static constexpr float kIRNextX = 404.0f;

    static bool inRect(float x, float y, float w, float h, double mx, double my)
    {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    }

    // A cream, gold-trimmed pill (the shared vintage control background).
    void creamPill(float x, float y, float w, float h)
    {
        beginPath();
        roundedRect(x, y, w, h, 5.0f);
        fillPaint(linearGradient(x, y, x, y + h, kColPanel, kColPanelLo));
        fill();
        strokeColor(kColGold);
        strokeWidth(1.5f);
        stroke();
        closePath();
    }

    void drawStepBtn(float x, const char* glyph)
    {
        creamPill(x, kCabY, kIRBtnW, kCabH);
        fontSize(16.0f);
        fillColor(kColText);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(x + kIRBtnW * 0.5f, kCabY + kCabH * 0.5f, glyph, nullptr);
    }

    void drawCabBar()
    {
        const float cy = kCabY + kCabH * 0.5f;

        // on/off toggle (red when on, like the pilot light)
        if (fCab)
        {
            beginPath();
            roundedRect(kCabTogX, kCabY, kCabTogW, kCabH, 5.0f);
            fillColor(kColAccent);
            fill();
            strokeColor(kColGold);
            strokeWidth(1.5f);
            stroke();
            closePath();
        }
        else
        {
            creamPill(kCabTogX, kCabY, kCabTogW, kCabH);
        }
        fontSize(12.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(fCab ? kColGrilleTx : kColTextDim);
        text(kCabTogX + kCabTogW * 0.5f, cy, fCab ? "CAB ON" : "CAB OFF", nullptr);

        // IR stepper: < [ IR NN ] >
        drawStepBtn(kIRPrevX, "<");
        drawStepBtn(kIRNextX, ">");

        const float dispX = kIRPrevX + kIRBtnW + 6.0f;
        const float dispW = kIRNextX - 6.0f - dispX;
        creamPill(dispX, kCabY, dispW, kCabH);

        char buf[32];
        std::snprintf(buf, sizeof(buf), "IR %02d", fCabIR + 1);
        fontSize(13.0f);
        fillColor(kColText);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(dispX + dispW * 0.5f, cy, buf, nullptr);
    }

    void setCab(int on)
    {
        fCab = on ? 1 : 0;
        editParameter(kParamCab, true);
        setParameterValue(kParamCab, static_cast<float>(fCab));
        editParameter(kParamCab, false);
        repaint();
    }

    void stepIR(int dir)
    {
        fCabIR = (fCabIR + dir + kNumIRs) % kNumIRs;
        editParameter(kParamCabIR, true);
        setParameterValue(kParamCabIR, static_cast<float>(fCabIR));
        editParameter(kParamCabIR, false);
        repaint();
    }

    // Returns true if the press was consumed by the cabinet bar.
    bool handleCabPress(double mx, double my)
    {
        if (inRect(kCabTogX, kCabY, kCabTogW, kCabH, mx, my)) { setCab(!fCab); return true; }
        if (inRect(kIRPrevX, kCabY, kIRBtnW, kCabH, mx, my))  { stepIR(-1);   return true; }
        if (inRect(kIRNextX, kCabY, kIRBtnW, kCabH, mx, my))  { stepIR(+1);   return true; }
        return false;
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

        // value arc (red needle colour)
        beginPath();
        arc(kd.cx, kd.cy, kKnobR, kArcStart, ang, NanoVG::CW);
        strokeColor(kColAccent);
        strokeWidth(4.0f);
        stroke();
        closePath();

        // brass knob body (radial highlight top-left)
        beginPath();
        circle(kd.cx, kd.cy, kKnobR - 6.0f);
        fillPaint(radialGradient(kd.cx - 6.0f, kd.cy - 8.0f, 2.0f, kKnobR - 2.0f, kColKnobHi, kColKnobLo));
        fill();
        closePath();
        beginPath();
        circle(kd.cx, kd.cy, kKnobR - 6.0f);
        strokeColor(kColGrilleDk);
        strokeWidth(1.5f);
        stroke();
        closePath();

        // pointer
        const float px = kd.cx + std::cos(ang) * (kKnobR - 8.0f);
        const float py = kd.cy + std::sin(ang) * (kKnobR - 8.0f);
        beginPath();
        moveTo(kd.cx, kd.cy);
        lineTo(px, py);
        strokeColor(kColKnobPtr);
        strokeWidth(2.5f);
        stroke();
        closePath();

        // value readout (cream over the grille, with a soft dark shadow)
        char buf[32];
        float shown = value;
        if (kd.index == kParamDryWet)
            shown = value * 100.0f;
        std::snprintf(buf, sizeof(buf), kd.fmt, shown);

        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fontSize(12.0f);
        fillColor(kColGrilleDk);
        text(kd.cx + 0.7f, kd.cy + kKnobR + 16.7f, buf, nullptr);
        fillColor(kColGrilleTx);
        text(kd.cx, kd.cy + kKnobR + 16.0f, buf, nullptr);

        // label
        fontSize(10.0f);
        fillColor(kColGrilleDk);
        text(kd.cx + 0.6f, kd.cy + kKnobR + 32.6f, kd.label, nullptr);
        fillColor(kColGrilleTx);
        text(kd.cx, kd.cy + kKnobR + 32.0f, kd.label, nullptr);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DirtyTalkUI)
};

UI* createUI()
{
    return new DirtyTalkUI();
}

END_NAMESPACE_DISTRHO

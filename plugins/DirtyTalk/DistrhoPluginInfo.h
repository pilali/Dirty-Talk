/*
 * Dirty Talk - Lo-fi vocal distortion
 * Copyright (c) 2026 Pilal - ISC license
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Pilal"
#define DISTRHO_PLUGIN_NAME    "Dirty Talk"
#define DISTRHO_PLUGIN_URI     "https://github.com/pilali/Dirty-Talk"
#define DISTRHO_PLUGIN_CLAP_ID "com.pilal.dirty-talk"

// 4-char identifiers, required by AUv2 / used by VST2 / VST3 / CLAP
#define DISTRHO_PLUGIN_BRAND_ID  Plal
#define DISTRHO_PLUGIN_UNIQUE_ID DtTk

// HAS_UI defaults to 1 (desktop formats ship the NanoVG GUI). The MOD/LV2
// device build overrides it to 0 (-DDISTRHO_PLUGIN_HAS_UI=0) so the DSP-only
// bundle carries no native ui:X11UI — the web modgui drives the UI there.
#ifndef DISTRHO_PLUGIN_HAS_UI
#define DISTRHO_PLUGIN_HAS_UI      1
#endif
#define DISTRHO_PLUGIN_IS_RT_SAFE  1
#define DISTRHO_PLUGIN_NUM_INPUTS  1
#define DISTRHO_PLUGIN_NUM_OUTPUTS 1
#define DISTRHO_PLUGIN_WANT_STATE  0

#define DISTRHO_UI_USE_NANOVG     1
#define DISTRHO_UI_DEFAULT_WIDTH  480
#define DISTRHO_UI_DEFAULT_HEIGHT 320

// Effect categorisation per format
#define DISTRHO_PLUGIN_LV2_CATEGORY   "lv2:DistortionPlugin"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Distortion|Mono"
#define DISTRHO_PLUGIN_CLAP_FEATURES  "audio-effect", "distortion", "mono"
#define DISTRHO_PLUGIN_AU_TYPE        aufx

enum Parameters {
    kParamMode = 0,
    kParamFreq,
    kParamBandwidth,
    kParamGate,
    kParamDryWet,
    kParameterCount
};

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED

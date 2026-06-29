# Dirty Talk

Lo-fi vocal distortion effect — emulates a **vintage microphone**, a **phone line**,
a **megaphone** or a **small speaker**. Built on the [DISTRHO Plugin Framework (DPF)](https://github.com/DISTRHO/DPF).

| | |
|---|---|
| **Brand** | Pilal |
| **License** | ISC |
| **I/O** | mono → mono |

## Signal chain

```
in → noise gate → band-pass (TPT SVF, input focus) → light compressor
   → distortion (2x oversampled) → device voicing (biquad cascade) → DC blocker → dry/wet → out
```

Each mode pairs a **distortion character** with a **device voicing** (a small
biquad cascade that models the transducer's frequency response — multiple
peaks/notches a single band-pass can't reproduce). The distortion stage is
**2x oversampled** to reduce aliasing. Mode levels are matched.

- **Mode** — Vintage Mic / Phone / Megaphone / Small Speaker
- **Center Freq** — input focus band-pass centre (300–3000 Hz, log)
- **Bandwidth** — band-pass Q (0.2–4)
- **Gate** — noise gate threshold (−60–0 dB), click-free smoothed gain
- **Dry/Wet** — smoothed mix

## Formats

From a single DSP source DPF produces, per platform:

| Format | Linux | Windows | macOS |
|---|:---:|:---:|:---:|
| LV2 | ✅ | ✅ | ✅ |
| VST2 / VST3 | ✅ | ✅ | ✅ |
| CLAP | ✅ | ✅ | ✅ |
| JACK standalone | ✅ | ✅ | ✅ |
| AudioUnit (AUv2) | — | — | ⚠️ experimental in DPF |

A sober vector GUI (DGL + NanoVG) ships with all desktop formats; a classic
HTML/CSS **modgui** ships with the MOD build.

## Building (desktop)

Requires a C++ compiler, `make`, `pkg-config` and OpenGL/X11 dev headers
(`libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev` on Debian/Ubuntu).

```sh
git clone --recursive https://github.com/pilali/Dirty-Talk.git
cd Dirty-Talk
make            # builds LV2, VST2, VST3, CLAP, JACK (+ AU on macOS)
make gen        # generates the LV2 .ttl files
```

Artifacts land in `bin/`. macOS universal binaries: `make MACOS_UNIVERSAL=true`.

> If you cloned without `--recursive`: `git submodule update --init --recursive`.

## Repository layout

```
plugins/DirtyTalk/   DSP, UI and build files
  DistrhoPluginInfo.h
  DirtyTalkPlugin.cpp   (DSP)
  DirtyTalkUI.cpp       (GUI - DGL/NanoVG)
modgui/              classic MOD web UI (HTML/CSS + assets + modgui.ttl)
mod-builder/         mod-plugin-builder package (dirty-talk.mk)
dpf/                 DISTRHO Plugin Framework (git submodule)
.github/workflows/   CI for Linux / Windows / macOS
```

## MOD Audio

`mod-builder/dirty-talk.mk` is a [mod-plugin-builder](https://github.com/moddevices/mod-plugin-builder)
package. Place it under `package/dirty-talk/`, add a matching `Config.in`, then add
`dirty-talk` to your build set.

The MOD bundle is built for a headless mod-host/mod-ui device, not the desktop:
it ships a **single DSP-only binary** (`dirty_talk.so`, built with
`DISTRHO_PLUGIN_HAS_UI=0` so there is no native `ui:X11UI`) and the web modgui.
`manifest.ttl` declares the plugin and `rdfs:seeAlso`s both `dirty_talk.ttl`
(ports) and `modgui.ttl` (web UI); `modgui.ttl` sits at the bundle root with its
resources under `modgui/`. The desktop formats above keep the embedded NanoVG GUI.

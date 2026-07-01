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
   → drive → distortion (2x oversampled) → device voicing (biquad cascade)
   → DC blocker → cabinet (IR convolution) → dry/wet → output → out
```

Each mode pairs a **distortion character** with a **device voicing** (a small
biquad cascade that models the transducer's frequency response — multiple
peaks/notches a single band-pass can't reproduce). The distortion stage is
**2x oversampled** to reduce aliasing. Mode levels are matched.

- **Mode** — Vintage Mic / Phone / Megaphone / Small Speaker
- **Center Freq** — input focus band-pass centre (300–3000 Hz, log)
- **Bandwidth** — band-pass Q (0.2–4)
- **Gate** — noise gate threshold (−60–0 dB), click-free smoothed gain
- **Drive** — saturation amount driven into the waveshaper (−12–+24 dB, smoothed)
- **Output** — output level applied to the final mix (−24–+24 dB, smoothed)
- **Dry/Wet** — smoothed mix
- **Cabinet** — on/off small-speaker impulse-response convolution
- **Cab IR** — one of 20 embedded small-speaker impulse responses

### Cabinet (IR convolution)

The optional **Cabinet** stage runs the signal through a real small-speaker
impulse response using a self-contained uniform-partitioned FFT convolver
(`IRConvolver.hpp`, no external dependency). The 20 embedded IRs are resampled
to the host rate at load time. Convolution adds a fixed one-block latency
(256 samples) which is reported to the host for delay compensation.

The impulse responses come from Jim's free "Small Speaker IR Pack"
(HippieLoveTurbo.com) — see [IR_CREDITS.md](IR_CREDITS.md). They are embedded as
a generated header (`plugins/DirtyTalk/DirtyTalkIRs.h`); regenerate with
`python3 tools/gen_irs.py <pack-dir>`.

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
make gen        # generates the LV2 .ttl files (desktop dsp+ui bundle)
make mod        # assembles the consolidated MOD bundle: bin/dirty-talk.lv2/
```

Artifacts land in `bin/`. macOS universal binaries (x86_64 + arm64, min macOS
10.15): `make macos-universal-10.15` — a DPF convenience target. The old
`MACOS_UNIVERSAL=true` variable is not understood by DPF and was a no-op.

`make mod` produces the headless device bundle locally — a single DSP-only
`dirty_talk.so`, generated `dirty_talk.ttl`, `modgui.ttl` and a `modgui/`
resource folder — identical in layout to what `mod-builder/dirty-talk.mk`
installs on a MOD device. It is what you copy into `~/.lv2` to test under
mod-host/mod-ui locally.

> If you cloned without `--recursive`: `git submodule update --init --recursive`.

## Repository layout

```
plugins/DirtyTalk/   DSP, UI and build files
  DistrhoPluginInfo.h
  DirtyTalkPlugin.cpp   (DSP)
  DirtyTalkUI.cpp       (GUI - DGL/NanoVG)
  IRConvolver.hpp       (partitioned FFT convolution engine)
  DirtyTalkIRs.h        (generated: embedded small-speaker IRs)
tools/gen_irs.py     regenerates DirtyTalkIRs.h from the source IR pack
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

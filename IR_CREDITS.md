# Impulse Response Credits

The **Cabinet** stage convolves the signal with impulse responses from the
**"Small Speaker IR Pack"** created and released by **Jim** of
**[HippieLoveTurbo.com](https://hippieloveturbo.com)** (host of a radio show on
KUCR 88.3FM, Riverside, California).

The pack captures a collection of tiny, lo-fi transducers — a bootleg Godzilla
alarm clock, an electronic Buddhist prayer box, a Sylvania transistor radio, a
Panasonic tape recorder, a Korg Monotron Delay, a "Music Pretty" keyboard, a
Furby, and assorted DC motors used as speakers — recorded with an AKG
Perception 170 and deconvolved in REAPER / ReaVerb.

## Terms

From the pack's own info sheet:

> Everything is free and there are no copyrights or usage terms. I'd prefer if
> you don't try to sell it though.

Out of respect for the author's wishes, **Dirty Talk is distributed as free,
non-commercial software** and these impulse responses are bundled for that use.
If you fork this project, please keep this attribution and honour the "please
don't sell it" request for the IR assets.

## How the IRs are used

The source WAVs (32-bit float, stereo, 44.1 kHz, ~4 s) are **not** committed to
this repository. They are processed into the embedded C array
`plugins/DirtyTalk/DirtyTalkIRs.h` by `tools/gen_irs.py`, which:

1. sums each IR to mono,
2. trims leading silence,
3. keeps the first 4096 taps (~93 ms — where a small speaker's colour lives),
4. applies a short tail fade, and
5. normalises to unit L2 energy so every IR sits at a comparable loudness.

The plugin resamples the stored 44.1 kHz IRs to the host sample rate at load
time and convolves via a uniform-partitioned FFT engine (`IRConvolver.hpp`).

To regenerate the header from the original pack:

```sh
python3 tools/gen_irs.py "/path/to/Small Speaker IR Pack - Hippie Love Turbo"
```

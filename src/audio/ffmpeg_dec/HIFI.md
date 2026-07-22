# HiFi intrinsics opportunities in ffmpeg_dec

Analysis of the hot audio-processing paths in the FFmpeg SOF module (decode /
filter / encode) and where Xtensa **HiFi** SIMD intrinsics would give the biggest
wins. It covers both the code this module owns and the FFmpeg code it links.

## Background: why this matters on Xtensa

- The FFmpeg archive is cross-built with **`--disable-asm`** (see `ffmpeg.cmake`),
  because FFmpeg has no Xtensa assembly. Every DSP kernel therefore runs as
  **scalar C** on the DSP — no SIMD, no fused multiply-add vectorisation.
- FFmpeg dispatches its DSP through function-pointer contexts with per-architecture
  init hooks (`ff_*_init_aarch64/arm/x86/riscv`) — but **no `_xtensa`** variant. The
  generic `_c` kernels are what execute on our target.
- SOF already ships HiFi-optimised DSP and the toolchain for it. Use it as the
  template:
  - ISA select + intrinsics header (`src/math/exp_fcn_hifi.c`):
    ```c
    #if   XCHAL_HAVE_HIFI5
    #include <xtensa/tie/xt_hifi5.h>
    #elif XCHAL_HAVE_HIFI4
    #include <xtensa/tie/xt_hifi4.h>
    #else
    #include <xtensa/tie/xt_hifi3.h>
    #endif
    ```
  - Existing HiFi kernels to mirror: `src/math/fir_hifi{2ep,3,5}.c`,
    `src/math/iir_df1_hifi{3,4,5}.c`, `src/math/fft/fft_{16,32}_hifi3.c`,
    `src/math/exp_fcn_hifi.c`. `ace30` (ptl) is HiFi4-class.

There are two distinct bodies of code:

- **(A) Module glue we own** — the PCM format-conversion loops in
  `ffmpeg_dec-*.c`. Small, per-sample, easy to hand-vectorise with HiFi, and we
  can change them freely.
- **(B) FFmpeg DSP** — the actual codec compute (MDCT/FFT, LPC, windowing). Much
  higher cycle cost, but upstream; adding HiFi means either FFmpeg fork patches
  (an `_xtensa` DSP init, mirroring the other arch dirs) or routing to SOF's own
  HiFi kernels.

## Hot-path summary

| # | Path | File | Kind | Now | HiFi win | Effort |
|---|------|------|------|-----|----------|--------|
| 1 | MDCT/FFT (AAC/MP3/Opus/afftdn) | FFmpeg `libavutil/tx*` | float/fixed transform | scalar C | **very high** | high |
| 2 | float vector kernels (fmul/window/overlap-add) | FFmpeg `libavutil/float_dsp` | float SIMD | scalar C | **high** | low–med |
| 3 | FLAC LPC / residual | FFmpeg `libavcodec/flacdsp` | int32/64 MAC | scalar C | high | med |
| 4 | MP3 synthesis window / imdct36 | FFmpeg `libavcodec/mpegaudiodsp` | float/fixed | scalar C | high | med |
| 5 | AAC PS / SBR DSP | FFmpeg `aacpsdsp`/`sbrdsp` | float SIMD | scalar C | med (HE-AAC only) | med |
| 6 | S32↔float PCM convert (filter) | `ffmpeg_dec-filter.c:230,255` | int↔float + scale | scalar C | med | **low** |
| 7 | planar→interleaved PCM (decode) | `ffmpeg_dec-ffmpeg.c:231` | copy/interleave | scalar C | low–med | **low** |
| 8 | S32→S16 pack (encode) | `ffmpeg_dec-encode.c:147` | narrow + interleave | scalar C | med | **low** |
| 9 | fast float libm (powf/exp2f/...) | `fastmathf.c` | scalar poly/Newton | scalar C | med (per-band) | med |

Priority order (cost/benefit): **2 → 6/7/8 → 3 → 1 → 4 → 9 → 5**. Start with the
easy, high-use float vector kernels (2) and the conversion loops we own (6–8);
tackle the transform (1) last because it is the most work despite the highest
raw cost.

## (A) Module conversion loops — we own these, do them first

These run **per sample over a whole frame** (typically 1024–4608 samples ×
channels per call), so they are genuinely hot and are trivial, self-contained HiFi
wins. Add HiFi versions guarded by `#if XCHAL_HAVE_HIFI*` with the current scalar
loop as the `#else` fallback.

1. **Filter S32→float deinterleave + normalise** — `ffmpeg_dec-filter.c:230`
   ```c
   ((float *)in->data[c])[i] = (float)src[i * ch + c] / FFMPEG_AF_S32_SCALE;
   ```
   HiFi: load 32-bit lanes, convert int32→float (`AE_FLOAT32`-family), multiply by
   the reciprocal scale, store to the per-channel plane. Deinterleave with
   strided/`AE_SEL` moves.

2. **Filter float→S32 interleave + denormalise + clip** — `ffmpeg_dec-filter.c:255`
   The scalar path multiplies, branches to clamp, then casts. HiFi does the
   float→int32 convert **with saturation** in one op (no per-sample branch), which
   is both faster and removes the mispredicted clamp branch.

3. **Encode S32→S16 narrow + deinterleave** — `ffmpeg_dec-encode.c:147`
   ```c
   int16_t s = (int16_t)(in[i * ch + c] >> 16);
   ```
   HiFi: pack 32→16 with rounding (`AE_ROUND16X4F32`-style) and store; SIMD handles
   4–8 samples per iteration.

4. **Decode planar→interleaved** — `ffmpeg_dec-ffmpeg.c:231`
   Per-element `memcpy`-equivalent; a HiFi interleave (vector load per plane,
   `AE_SEL` interleave, vector store) removes the per-sample loop overhead. Lower
   priority as many decoders already output the sink format.

A single small `ffmpeg_dec-convert.c` with HiFi + scalar variants (like SOF's
`*_generic.c` / `*_hifi3.c` split) would hold all four and be reusable across the
three modes.

## (B) FFmpeg DSP — the real compute

FFmpeg's DSP contexts are function-pointer tables initialised per arch. The
Xtensa-idiomatic fix is a **fork patch** adding `ff_<dsp>_init_xtensa()` with HiFi
intrinsics, mirroring `libavcodec/aarch64/`, `x86/` etc. (our FFmpeg is already a
SOF fork — see `west.yml` — so such patches have a home).

- **`AVFloatDSPContext`** (`libavutil/float_dsp.h`) — `vector_fmul`,
  `vector_fmul_window`, `vector_fmul_add`, `vector_fmul_reverse`,
  `scalarproduct_float`. Used for windowing / overlap-add by **AAC, MP3 and Opus**.
  These are plain multiply/MAC over contiguous float arrays — the easiest and
  highest-leverage HiFi target (one small `float_dsp_init_xtensa` benefits three
  codecs). **Do this first on the FFmpeg side.**
- **`FLACDSPContext`** (`libavcodec/flacdsp.h`) — `lpc16`/`lpc32`/`lpc33` and
  decorrelate. FLAC decode is dominated by the LPC prediction MAC loop over the
  residual: a natural fit for HiFi multiply-accumulate. `ff_flacdsp_init_xtensa`.
- **`MPADSPContext`** (`libavcodec/mpegaudiodsp.h`) — `apply_window_{float,fixed}`,
  `imdct36_blocks_*`, `synth_filter`. The MP3 subband synthesis window is the MP3
  decode hot loop.
- **`libavutil/tx`** (MDCT/FFT) — the single largest cost for AAC/MP3/Opus decode
  and for the `afftdn` filter. It is a "codelet" system rather than one function
  pointer, so it is the most work. Two routes:
  1. Add HiFi FFT/MDCT codelets to the fork (largest effort, cleanest for FFmpeg).
  2. Bridge to **SOF's existing HiFi3 FFT** (`src/math/fft/fft_*_hifi3.c`) by
     replacing FFmpeg's transform behind `av_tx_init` for the sizes SOF supports.
     Less code, but a semantic bridge (twiddle/scaling/layout must match) and only
     covers power-of-two sizes.
- **`PSDSPContext` / SBR DSP** — only relevant if HE-AAC (SBR/PS) is enabled; skip
  unless needed.

## (C) fastmathf — the module's float libm

`fastmathf.c` (`powf`/`exp2f`/`log2f`/`sinf`/`cosf`/`sqrtf`/`cbrtf`) is scalar
polynomial/Newton code. These are called per-band / per-frame (not the innermost
per-sample loop), so they are a secondary target. HiFi wins come from:
- `sqrtf`/`cbrtf`: HiFi `RSQRT`/reciprocal seed instructions instead of the
  bit-trick + Newton.
- `exp2f`/`log2f`/`sinf`/`cosf`: process 2/4 lanes at once when a decoder needs a
  vector of them (e.g. AAC scalefactor gains), and use FMA for the polynomials.
  Mirror `src/math/exp_fcn_hifi.c`.

## Appendix: AAC-LC decode hot spots (per-frame detail)

Where the cycles actually go decoding one AAC-LC frame (1024 samples long, or
8×128 short) per channel, mapped to the code in the tree. Everything below runs as
**scalar C** on Xtensa today (`--disable-asm`, no `_xtensa` DSP init).

### Toolchain requirement (applies to all FFmpeg-side HiFi work)

The Cadence HiFi intrinsic headers (`<xtensa/tie/xt_hifi{3,4,5}.h>`) and a
`core-isa.h` with `XCHAL_HAVE_HIFI4 == 1` are **only** available through the
**LLVM/xt-clang Xtensa** toolchain (on this build host under
`llvm-project/.../clang/lib/Headers/xtensa/tie/` plus the ace30 core config at
`modules/hal/xtensa/.../soc/intel_ace30_ptl/xtensa/config/core-isa.h`). The
Zephyr-SDK GCC that `ffmpeg.cmake` currently drives does **not** ship these headers.

Consequence: the `ff_*_init_xtensa` kernels must be written with `#if
XCHAL_HAVE_HIFI*` intrinsics **and** a scalar `#else` fallback. Under the current
GCC cross-build the guard is 0, so the fallback compiles (correct, but no speedup).
Actual HiFi codegen requires cross-building FFmpeg with the LLVM Xtensa clang and
adding the ace30 core-config include dir to the FFmpeg `CFLAGS` — the same
toolchain SOF's own `src/math/*_hifi*.c` already rely on.

### Tier 1 — dominant, best return

1. **IMDCT** — `mdct1024_fn` / `mdct128_fn` (short blocks) via `libavutil/tx`,
   driven from `imdct_and_windowing()` (`libavcodec/aac/aacdec_dsp_template.c:341-343`).
   One 1024-pt (or 8×128) inverse MDCT per channel per frame — the single largest
   compute. Dispatch: `AVTXContext`. HiFi = complex-FMA butterflies, via HiFi `tx`
   codelets or a bridge to SOF's HiFi3 FFT (`src/math/fft/`). Highest cost, most work.

2. **`AVFloatDSPContext` vector kernels** — pervasive for windowing / overlap-add /
   stereo, and the easiest high-leverage target:
   - `vector_fmul_window` — windowed overlap-add, ~10 call sites (`:354-419`); *the*
     per-sample AAC output kernel.
   - `vector_fmul` / `vector_fmul_reverse` — window application (`:235-308`).
   - `vector_fmul_scalar` — intensity-stereo gain (`:146`).
   - `butterflies_float` — M/S stereo (`:102`).
   One `ff_float_dsp_init_xtensa` (in `libavutil/`) covers all of them and also
   accelerates MP3 and Opus. **Start here** (lowest effort, cross-codec).

### Tier 2 — AAC-specific loops (in `aacdec_dsp_template.c`, HiFi'd inline in the fork)

3. **Inverse quantization `x^(4/3)`** — `decode_spectrum_and_dequant()`
   (`libavcodec/aac/aacdec.c:1768`) + `dequant_scalefactors()` (`:41`): up to 1024
   coeffs, each `ff_cbrt_tab` lookup for `|x|^(4/3)` × scalefactor gain
   `2^(0.25·sf)`. HiFi = vectorised table gather + FMA (per-SFB gain is a
   `vector_fmul_scalar`).
4. **TNS** — `apply_tns()` (`:164`): LPC all-pole filter (order ≤ ~20) over spectral
   coeffs per window. HiFi = MAC filter (mirror `src/math/iir_df1_hifi*`).

### Tier 3 — HE-AAC only (skip unless SBR/PS enabled)

5. **SBR QMF** — `libavcodec/sbrdsp.c`: `sbr_qmf_pre/post_shuffle`, `sbr_hf_gen`,
   `sbr_hf_g_filt`, `sbr_sum64x5` + a 64-pt QMF transform. Dispatch:
   `AACSBRDSPContext` (`sbrdsp.h`, `ff_sbrdsp_init`). Nearly as heavy as the base IMDCT.
6. **PS** — `libavcodec/aacps.c` `hybrid_analysis` / `stereo_interpolate` via
   `PSDSPContext` (`aacpsdsp.h`, `ff_psdsp_init`).

### Where the HiFi hooks land + priority

| Priority | Target | Dispatch / location | Effort |
|---|--------|---------------------|--------|
| 1 | `vector_fmul_window` + friends | `AVFloatDSPContext` → `ff_float_dsp_init_xtensa` (libavutil) | low; AAC+MP3+Opus |
| 2 | IMDCT (mdct128/1024) | `libavutil/tx` — HiFi codelets or SOF-FFT bridge | high |
| 3 | dequant `x^(4/3)` | `aac/aacdec.c` + dsp_template (cbrt gather+FMA) | med |
| 4 | TNS | `apply_tns` (LPC MAC) | med |
| 5 | SBR / PS | `sbrdsp`/`aacpsdsp` `_xtensa` inits | med, HE-AAC only |

AAC-LC order: **float_dsp (2) → IMDCT (1) → dequant (3) → TNS (4)**; SBR/PS only if
HE-AAC. `float_dsp`/`tx`/`sbrdsp`/`psdsp` are `libavutil`/`libavcodec` and take
`_xtensa` inits mirroring the existing `aarch64`/`x86` dirs; dequant/TNS are AAC
decoder code patched in place. All on the `lgirdwood/FFmpeg` `sof-hifi` branch.

## Recommendations

1. **Quick wins we own** — hand-vectorise the four conversion loops (§A) with
   `#if XCHAL_HAVE_HIFI*` + scalar fallback. Small, self-contained, no fork needed.
2. **Biggest FFmpeg leverage-per-effort** — add `ff_float_dsp_init_xtensa` (HiFi
   `vector_fmul*`/`scalarproduct`) to the FFmpeg fork; benefits AAC + MP3 + Opus.
3. **Per-codec kernels** — `flacdsp` (FLAC), `mpegaudiodsp` (MP3) HiFi inits as
   fork patches, gated on the enabled decoders.
4. **Transform** — evaluate SOF-HiFi-FFT bridge vs. HiFi tx codelets for
   `libavutil/tx`; highest payoff, most work, do once the cheaper items land.
5. Always keep the scalar C path as the fallback (`#else`) and behind the same ISA
   guards SOF uses, so non-HiFi / host/testbench builds still work.

Measurement: profile on hardware per codec before and after; the ranking above is
by expected cost, but the actual hot spot depends on the codec and content (FLAC
is LPC-bound, AAC/MP3/Opus are transform+window-bound).

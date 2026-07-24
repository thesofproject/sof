# Testing the ffmpeg_dec module

This describes how to verify the FFmpeg FLAC decoder module end to end. FLAC is
lossless, so a correct decode is **bit-exact** against a reference decode.

## Test vectors

Generate vectors with the helper (needs `ffmpeg` on the host):

```
tools/test/audio/ffmpeg_dec_prepare.sh -i song.wav -o vectors/
```

Produces in `vectors/`:
- `streaminfo.bin` — 34-byte FLAC STREAMINFO = the codec **extradata** blob to
  load into the module's bytes control (`set_configuration`).
- `stream.raw` — the raw FLAC elementary stream fed to the decoder input.
- `ref_s32le.raw` — reference interleaved S32_LE PCM; the decoder output must
  match this byte-for-byte.

## Topology

The component widget is `tools/topology/topology2/include/components/ffmpeg_dec.conf`
(models the DTS codec: `type "effect"`, one input/one output pin, a `bytes`
control for STREAMINFO). Its `uuid` must match the firmware's registry entry for
`ffmpeg_dec` (see `uuid-registry.txt`).

A runnable pipeline must deliver **compressed bytes** to the module and take PCM
out. Two paths:

1. **ALSA compress-offload** (the production path for a decoder): the host writes
   the FLAC elementary stream to a compress DMA gateway; the module decodes; PCM
   goes to a DAI or host capture. This is the correct model (a decoder is not a
   fixed-rate PCM effect) and is the remaining integration to wire on real ptl
   hardware.
2. **Direct raw-data injection** (bring-up shortcut): a minimal pipeline where a
   host copier delivers `stream.raw` as an opaque byte buffer into the module's
   `process_raw_data`, capturing PCM on the other side.

## Verification paths

### A. On target (ptl hardware) — full verification
1. Build + flash firmware with the real backend:
   `CONFIG_COMP_FFMPEG_DEC=m`, `CONFIG_COMP_FFMPEG_DEC_STUB=n` (requires the
   FFmpeg static libs in `third_party/`; see the module README).
2. Load the topology; set the STREAMINFO bytes control:
   `amixer -c0 cset name='FFMPGDEC...' -- $(od -An -tx1 vectors/streaminfo.bin)`
   (or via `tinymix`).
3. Play `stream.raw` through the pipeline, capture the PCM output.
4. `cmp captured.raw vectors/ref_s32le.raw` — must be identical.

### B. Native testbench (no hardware) — decode-only check
The module builds native (built-in, real libc — the LLEXT-only shims/alloc are
excluded automatically, see `CMakeLists.txt`). Linking the **host** libavcodec
and driving `process_raw_data` over `stream.raw` with `ref_s32le.raw` as the
oracle gives a bit-exact check without a DSP. This harness is not yet wired (the
testbench file component is PCM-oriented; feeding compressed bytes needs a small
adapter) and is the recommended next step for CI-friendly verification.

## Status

- Component widget conf: **done**.
- Host vector/reference tooling: **done** (`ffmpeg_dec_prepare.sh`).
- Runnable pipeline (compress-offload) + on-target bit-exact run: **pending
  hardware**.
- Native testbench harness: **pending** (small compressed-input adapter).

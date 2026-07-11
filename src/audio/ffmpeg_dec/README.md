# FFmpeg audio decoder module (ffmpeg_dec)

Wraps FFmpeg's `libavcodec` audio decoders behind the SOF `module_interface`,
decoding a compressed elementary stream to PCM inside the DSP. First target
codec is **FLAC**.

## Design

The SOF glue and the decoder are separated so the integration can be validated
without libavcodec:

- `ffmpeg_dec.c` — SOF module core: `init` / `prepare` / `process_raw_data` /
  `set_configuration` / `reset` / `free`, plus the LLEXT manifest. Input is the
  compressed byte stream (raw data), output is interleaved PCM. Decoding is
  delegated to a `struct ffmpeg_dec_backend`.
- `ffmpeg_dec-stub.c` — dependency-free passthrough backend. Lets CI build and
  exercise the module (pipeline, IPC config, LLEXT packaging) with **no** FFmpeg
  libraries. Selected by `CONFIG_COMP_FFMPEG_DEC_STUB`.
- `ffmpeg_dec-ffmpeg.c` — real backend driving the standalone `libavcodec`
  send-packet / receive-frame API. A raw stream is framed on-DSP with an
  `AVCodecParser` (`av_parser_parse2`); decode is forced single-threaded
  (`thread_count = 1`). Requires pre-compiled static libraries and headers under
  `third_party/` (see below).
- `ffmpeg_dec.h` — private data (`struct ffmpeg_dec_comp_data`) and the backend
  interface.

The codec setup header (e.g. FLAC STREAMINFO) is delivered as a binary control
via `set_configuration` and stored as `extradata`, which the libavcodec backend
installs before `avcodec_open2()`.

## Build

- **Stub (default for testing/CI):** `CONFIG_COMP_FFMPEG_DEC=y` (or `=m` for
  LLEXT) with `CONFIG_COMP_FFMPEG_DEC_STUB=y`. No external dependencies.
- **Real decoder:** `CONFIG_COMP_FFMPEG_DEC=m`, `CONFIG_COMP_FFMPEG_DEC_STUB=n`.
  Requires cross-compiled decoder-only FFmpeg static libraries installed as:
  - `third_party/lib/libavcodec.a`, `libavutil.a`, `libswresample.a`
  - `third_party/include/libavcodec/…`, `libavutil/…`, `libswresample/…`
  Configure FFmpeg with `--disable-everything --disable-avformat
  --disable-pthreads --enable-decoder=flac --enable-parser=flac` (plus the
  target cross-compile flags).

## Files

- `Kconfig` — `COMP_FFMPEG_DEC` (tristate) and `COMP_FFMPEG_DEC_STUB`.
- `CMakeLists.txt` / `llext/CMakeLists.txt` — static and LLEXT builds; the LLEXT
  target name is `ffmpeg_dec` and links the FFmpeg libraries in the non-stub
  branch.
- `ffmpeg_dec.toml` / `llext/llext.toml.h` — rimage module manifest
  (`UUIDREG_STR_FFMPEG_DEC`); OBS is sized above IBS since PCM out ≫ compressed
  in.

## Status / TODO

- Codec id is currently hard-coded to FLAC in `init`; wire it from topology/IPC
  init config for other codecs.
- Output is assumed to already match the sink sample format; add
  `libswresample` (or a fixed-point path) for format/rate conversion.
- Topology and host test tooling for end-to-end (bit-exact) FLAC decode.

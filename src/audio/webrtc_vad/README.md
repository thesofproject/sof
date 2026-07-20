# WebRTC Voice Activity Detection module (`webrtc_vad`)

Wraps [libfvad](https://github.com/dpirch/libfvad) — a standalone pure-C
BSD-3 extraction of the WebRTC GMM Voice Activity Detection algorithm — behind
the SOF `module_interface`.

## What it does

The module is a **pass-through PCM effect**: audio is copied from source to
sink without modification. On each complete VAD frame (10, 20 or 30 ms,
Kconfig-selectable), the module classifies channel-0 audio as speech or
non-speech using the libfvad GMM classifier and broadcasts the binary decision
(`WEBRTC_VAD_SPEECH` / `WEBRTC_VAD_SILENCE`) via `NOTIFIER_ID_VAD`.

Downstream consumers (keyword detection gating, host-side VAD forwarding,
noise suppression activation) subscribe to this notifier without needing to
poll the module.

## Algorithm

libfvad implements the WebRTC Gaussian Mixture Model VAD:
- Operates in **Q15 fixed-point** — no FPU required
- Internally resamples any 8/16/32/48 kHz input to **8 kHz** for classification
- 6 sub-band log-energy features feed a per-band GMM; bands are aggregated to
  a single speech/non-speech bit
- Four aggressiveness modes (0 = most conservative, 3 = most aggressive)

## Design

```
webrtc_vad.c       — SOF module_interface glue (init/prepare/process/reset/free)
webrtc_vad.h       — private data (struct webrtc_vad_comp_data) + backend API
webrtc_vad-stub.c  — dependency-free passthrough stub; always reports SPEECH
webrtc_vad-fvad.c  — real libfvad backend
webrtc_vad-shims.c — malloc/free/mem* shims for LLEXT (backed by rballoc)
webrtc_vad.cmake   — ExternalProject cross-build of libfvad static library
```

The `struct webrtc_vad_backend` interface mirrors `struct ffmpeg_dec_backend`:
a named set of function pointers (`init`, `configure`, `classify`, `reset`,
`free`). The SOF glue calls only these and remains independent of libfvad.

## Build

**Stub (CI/testing):**
```
CONFIG_COMP_WEBRTC_VAD=y   # or =m for LLEXT
CONFIG_COMP_WEBRTC_VAD_STUB=y
```
No external dependencies. The stub always reports speech.

**Real libfvad backend:**
```
CONFIG_COMP_WEBRTC_VAD=m    # LLEXT
CONFIG_COMP_WEBRTC_VAD_STUB=n
```
Requires `west update` to pull `modules/audio/libfvad` (pinned in
`west.yml`). The cross-build is driven by `webrtc_vad.cmake` invoked from
`llext/CMakeLists.txt`.

## Kconfig options

| Option | Default | Description |
|---|---|---|
| `COMP_WEBRTC_VAD` | — | Enable module (y=built-in, m=LLEXT) |
| `COMP_WEBRTC_VAD_STUB` | y if COMP_STUBS | Use passthrough stub backend |
| `WEBRTC_VAD_MODE` | 2 | Aggressiveness 0–3 |
| `WEBRTC_VAD_FRAME_MS` | 10 | VAD frame size in ms (10/20/30) |

## Frame accumulation

SOF pipeline periods may be shorter than the VAD frame size. The module
accumulates channel-0 S16 samples in a ring buffer (`accumulator[]`) and
calls `classify()` once per full frame, introducing at most `WEBRTC_VAD_FRAME_MS`
ms of additional latency.

## Status / TODO

- Topology wiring for IPC4 (`.toml` mod_cfg tuning for target platform)
- `set_configuration` handler for runtime mode/frame-ms changes via IPC
- Forward VAD decision to host via a SOF IPC notification message
- Unit test with reference speech/noise WAV files in `west twister`

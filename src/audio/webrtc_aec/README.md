# WebRTC Acoustic Echo Cancellation Module (`webrtc_aec`)

Wraps WebRTC's Acoustic Echo Cancellation algorithms behind the SOF `module_interface`. Supports classic fixed-point AECm, classic floating-point MDF AEC, and modern state-of-the-art WebRTC AEC3 (C++17) with stereo 2x2 MIMO processing.

---

## Features

- **Tri-Backend Architecture**:
  - **Modern AEC3 (C++17)**: State-of-the-art WebRTC AEC3 engine featuring matched filter delay estimation, multi-partition frequency-domain adaptive filtering, non-linear echo suppression, comfort noise generation, and ERLE tracking. Optimized for Intel ACE 3.0 (Panther Lake).
  - **Classic Float AEC (MDF)**: Single-precision floating-point Multidelay Block Frequency Domain adaptive filter (`echo_cancellation.c`) for FPU-enabled DSPs.
  - **Classic Fixed-Point AECm**: Q15 integer echo canceller (`echo_control_mobile.c`) for low-power DSPs without hardware FPU.
  - **Pass-Through Stub**: Zero-dependency pass-through for CI and staging pipeline validation.
- **Multi-Channel & Asymmetric Echo Cancellation ($R\text{R} \times C\text{C}$, up to $4 \times 4$)**:
  - Decoupled reference (`ref_channels`) and microphone (`mic_channels`) counts (`WEBRTC_AEC_CHANNELS_MAX = 4`), eliminating ch0 output cloning.
  - Supports $1\text{R} \times N\text{C}$, $2\text{R} \times 2\text{C}$, asymmetric $2\text{R} \times 1\text{C}$ / $2\text{R} \times 4\text{C}$, and optional multi-channel reference downmixing (`CONFIG_WEBRTC_AEC_STEREO_DOWNMIX_REF`).
  - **Multi-Rate Pipeline Support**: Native 8 kHz / 16 kHz plus WebRTC SPL resampling for 32 kHz (`WebRtcSpl_DownsampleBy2` / `UpsampleBy2`) and 48 kHz (`WebRtcSpl_Resample48khzTo16khz` / `16khzTo48khz`).
  - **AEC3 Stereo Power Tracking & Suppression**: Full $R \times C$ `EchoCanceller3::CreateDefaultMultichannelConfig()` support with configurable filter length, coarse filter fallback, stereo content detector, and external/internal delay estimation.
- **DSP Mathematical Accelerations**:
  - **Fast Ooura RDFT**: 128-point split-radix real discrete Fourier transform replacing generic recursive FFTs.
  - **Branchless Soft-Float**: Fast mantissa seed lookup + Newton-Raphson iterations (`FastFloatSqrt`, `FastFloatDiv`).
  - **Zero Heap Allocations**: All frame buffers and filter states pre-allocated in static DSP heap during initialization.
- **Cycle Telemetry**: Built-in microsecond cycle telemetry (`CONFIG_WEBRTC_AEC_PERF_TELEMETRY`) outputting real-time phase breakdowns (`[AEC3_P1]`, `[AEC3_P2]`).

---

## Architecture & Data Flow

### Modern AEC3 Stereo 2x2 MIMO Pipeline

```mermaid
graph TD
    %% Input Streams
    RefStereo["Speaker Playback Reference (Pin 1)<br/>Stereo S16/S32 PCM"] --> Core[webrtc_aec.c Core]
    MicStereo["Microphone Capture (Pin 0)<br/>Stereo S16/S32 PCM"] --> Core

    %% Framing & Normalization
    Core -->|Accumulate 4ms Blocks| FrameBuf[10ms / 4ms FIFO Buffer]
    FrameBuf -->|Normalize ±1.0f| AEC3Wrapper[webrtc_aec3_wrapper.cpp]

    %% Render Path
    AEC3Wrapper -->|AnalyzeRender| RenderEngine[AEC3 Render Processor]
    RenderEngine -->|Downmix 2ch -> 1ch| RenderMono[Virtual Mono Reference]
    RenderMono -->|128-pt RDFT| RenderFreq[Render Frequency Spectra X]

    %% Capture Path
    AEC3Wrapper -->|ProcessCapture| CaptureEngine[AEC3 Capture Processor]
    CaptureEngine -->|Linear Subtraction| AdaptiveFilter[MDF Linear Adaptive Filter]
    RenderFreq --> AdaptiveFilter
    AdaptiveFilter -->|Error Signals e1, e2| Suppressor[Nonlinear Echo Suppressor]

    %% Residual Echo Suppression & Output
    Suppressor -->|Stereo Power Tracking| SpectralMask[Joint Suppression Mask]
    SpectralMask -->|Synthesize Time Domain| CleanOut[Clean Stereo Output]
    CleanOut -->|Denormalize PCM| Core
    Core --> CleanStereo["Echo Cancelled Output (Pin 0)<br/>Stereo S16/S32 PCM"]

    %% Diagnostics
    AEC3Wrapper -.->|Cycle Count| Telemetry["Telemetry ([AEC3_P1] / [AEC3_P2])"]
```

### Components

- `webrtc_aec.c`: Core SOF module interface wrapper handling dual input pin binding, period accumulation, rate validation, and format translation.
- `webrtc_aec-aec3.c`: C translation unit interfacing SOF with the modern C++17 AEC3 engine.
- `webrtc_aec3_wrapper.cpp`: C++17 wrapper managing `webrtc::EchoCanceller3` and `webrtc::AudioBuffer` lifecycles.
- `webrtc_aec-math.c`: Fast 128-point Ooura RDFT and branchless soft-float kernel implementations.
- `webrtc_aec-aec.c`: Classic floating-point MDF AEC backend.
- `webrtc_aec-aecm.c`: Classic fixed-point Q15 AECm backend.
- `webrtc_aec-stub.c`: Pass-through stub backend.

---

## Build Configuration

### 1. Modern WebRTC AEC3 (Panther Lake / ACE 3.0)

```ini
CONFIG_COMP_WEBRTC_AEC=y
CONFIG_COMP_WEBRTC_AEC_STUB=n
CONFIG_WEBRTC_AEC_BACKEND_AEC3=y
CONFIG_WEBRTC_AEC_PERF_TELEMETRY=y
```

### 2. Classic Float AEC (MDF)

```ini
CONFIG_COMP_WEBRTC_AEC=y
CONFIG_COMP_WEBRTC_AEC_STUB=n
CONFIG_WEBRTC_AEC_BACKEND_FLOAT=y
```

### 3. Classic Fixed-Point AECm (Mobile / Low-Power Integer)

```ini
CONFIG_COMP_WEBRTC_AEC=y
CONFIG_COMP_WEBRTC_AEC_STUB=n
CONFIG_WEBRTC_AEC_BACKEND_AECM=y
CONFIG_WEBRTC_AEC_FILTER_LEN_MS=64
CONFIG_WEBRTC_AEC_SUPPRESSION_LEVEL=1
```

---

## Kconfig Parameters

| Option | Default | Type / Range | Description |
|---|---|---|---|
| `CONFIG_COMP_WEBRTC_AEC` | n | tristate (`y`/`m`/`n`) | Enable WebRTC AEC module |
| `CONFIG_COMP_WEBRTC_AEC_STUB` | y | bool | Build pass-through stub |
| `CONFIG_WEBRTC_AEC_BACKEND_AEC3` | y | bool | Use modern WebRTC AEC3 (C++17) backend |
| `CONFIG_WEBRTC_AEC_BACKEND_FLOAT` | n | bool | Use classic floating-point MDF AEC backend |
| `CONFIG_WEBRTC_AEC_BACKEND_AECM` | n | bool | Use classic fixed-point integer AECm backend |
| `CONFIG_WEBRTC_AEC_PERF_TELEMETRY` | y | bool | Enable real-time cycle profiling via printk |
| `CONFIG_WEBRTC_AEC_FILTER_LEN_MS` | 64 | int (`32`, `64`, `128`) | AECm adaptive filter length in ms |
| `CONFIG_WEBRTC_AEC_SUPPRESSION_LEVEL` | 1 | int (`0` - `2`) | AECm comfort-noise suppression level |
| `CONFIG_WEBRTC_AEC_SAMPLE_RATE_HZ` | 16000 | int (`8000`, `16000`) | Native AEC processing rate (32/48 kHz resampled) |
| `CONFIG_WEBRTC_AEC_DELAY_MS` | 20 | int (`0` - `500`) | Nominal echo path / soundcard buffer delay in ms |
| `CONFIG_WEBRTC_AEC_STEREO_DOWNMIX_REF` | n | bool | Downmix multi-channel reference to mono before AEC |
| `CONFIG_WEBRTC_AEC_STEREO_CONTENT_DETECT` | y | bool (AEC3) | Dynamic stereo vs. dual-mono content detector |
| `CONFIG_WEBRTC_AEC_AEC3_EXTERNAL_DELAY_EST` | n | bool (AEC3) | Lock AEC3 to `WEBRTC_AEC_DELAY_MS` instead of matched filter |
| `CONFIG_WEBRTC_AEC_AEC3_FILTER_BLOCKS` | 12 | int (`1` - `30`, AEC3) | AEC3 refined/coarse adaptive filter length in 4 ms blocks |
| `CONFIG_WEBRTC_AEC_AEC3_COARSE_FILTER` | y | bool (AEC3) | Enable AEC3 coarse filter output fallback |
| `CONFIG_WEBRTC_AEC_DELAY_AGNOSTIC` | n | bool (Float) | Enable signal-based delay estimation and extended filter |

---

## Usage & Topology

### Topology Connections

The AEC module requires two input pins:
1. **Pin 0 (Capture)**: Fed by the Microphone capture DAI copier.
2. **Pin 1 (Reference)**: Fed by the Speaker playback reference copier (cross-pipeline).

```
[ Mic DAI Copier (Capture) ] ----------> Pin 0 |
                                               | [ webrtc_aec ] ---> [ Host Copier ]
[ Speaker DAI Copier (Playback Ref) ] -> Pin 1 |
```

#### Topology 2 Code Snippet

```alsa
Object.Widget.webrtc-aec.1 {
    # Pin 0: Microphone Capture
    Object.Base.input_pin_binding.1 {
        input_pin_binding_name "dai-copier.SSP.NoCodec-0.capture"
    }
    # Pin 1: Speaker Playback Reference
    Object.Base.input_pin_binding.2 {
        input_pin_binding_name "dai-copier.SSP.NoCodec-2.capture"
    }
}
```

---

## Performance & MCPS Profile (Aphid / Panther Lake ACE 3.0)

Evaluated directly on Intel Panther Lake (`ptl` / `intel_ace30_ptl`) Aphid hardware at **400 MHz** core clock:

| Backend Mode | Channel Topology | Block Size | Core MCPS (Aphid) | End-to-End Pipeline MCPS | Duty Cycle (@ 400 MHz) |
|---|---|---|:---:|:---:|:---:|
| **Modern AEC3** | Stereo Ref $\times$ Stereo Mic ($2\text{R} \times 2\text{C}$) | 4 ms (64 smp @ 16 kHz) | **221.4 MCPS** | **280.2 MCPS** | **55.3%** |
| **Modern AEC3** | Mono Ref $\times$ Mono Mic ($1\text{R} \times 1\text{C}$) | 4 ms (64 smp @ 16 kHz) | **148.0 MCPS** | **182.5 MCPS** | **37.0%** |
| **Classic Float AEC** | Mono Ref $\times$ Mono Mic ($1\text{R} \times 1\text{C}$) | 10 ms (160 smp @ 16 kHz) | **42.0 MCPS** | **55.0 MCPS** | **10.5%** |
| **Classic AECm** | Mono Ref $\times$ Mono Mic ($1\text{R} \times 1\text{C}$) | 10 ms (160 smp @ 16 kHz) | **12.0 – 16.5 MCPS** | **18.0 MCPS** | **3.5%** |

### Aphid Hardware Telemetry Breakdowns

- **AEC3 Stereo $2\text{R} \times 2\text{C}$ Execution**:
  - Render analysis (`[AEC3_P1]`): ~18.2 k-ticks / 4ms block (~45.5 MCPS).
  - Capture analysis & echo cancellation (`[AEC3_P2]`): ~70.4 k-ticks / 4ms block (~175.9 MCPS).
  - Total block cycle budget: **~88.6 k-ticks per 4ms block** ($2.21\text{ ms}$ processing time per $4.0\text{ ms}$ frame).
- **Buffer Integrity**: Tested with `arecord -D hw:0,1 -r 16000 -c 2 -f S16_LE` under active playback. Recorded wall-clock time was $1.717\text{ s}$ with **zero underruns (0 xruns)**.

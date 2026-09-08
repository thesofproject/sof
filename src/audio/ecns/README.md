# ECNS (Echo Cancellation & Noise Suppression) Processing Module

## Overview

The `ecns` component is an IPC4 Data Processing (DP) module operating at a 20 ms period (320 samples at 16 kHz). It consumes a 4-channel audio stream comprising:
- **Channels 0 & 1**: Primary microphone inputs.
- **Channels 2 & 3**: Far-end echo reference inputs.

The module performs echo cancellation and noise suppression, delivering two distinct output streams via separate output pins:
1. **Output Pin 0 (Mono Clean Mic)**: Provides a single-channel, echo-canceled and noise-suppressed speech stream intended for keyword detection and pre-roll buffering in the Key Phrase Buffer (`kpb`).
2. **Output Pin 1 (Stereo Clean Mic)**: Provides a two-channel clean speech stream routed directly or via a host mixin/mixout bridge to a standard ALSA capture device (e.g. `hw:0,10`).

---

## Topology Architecture

```mermaid
graph LR
    subgraph Capture["Pipeline 100 — 4ch DMIC"]
        DAI["DAI Copier (4ch 16kHz)"] --> M100["mixin 100.1"]
    end

    subgraph ECNS_DP["Pipeline 105 — ECNS DP Module (20ms / 320 frames)"]
        M100 --> MO105["mixout 105.1"]
        MO105 --> ECNS["ecns.105.1\n(UUID: 214f8a6c-493a-4b1e-8b1e0d3b6f8a2c15)"]
        ECNS -- "Pin 0 (1ch Mono)" --> MIX105_1["mixin 105.1 (KPB Mixin)"]
        ECNS -- "Pin 1 (2ch Stereo)" --> MIX105_2["mixin 105.2 (Host Mixin)"]
    end

    subgraph WOV_Path["Pipeline 106 & 104 — WOV / KPB Path"]
        MIX105_1 --> MO106["mixout 106.1"]
        MO106 --> KPB["kpb.106.1\n(2.0s mono / 64 KB)"]
        KPB --> WOV["WOV Slots 0..2"] --> ARB["wov_arbiter.104.1"]
        ARB --> HC11["host-copier.11\n(ALSA hw:0,11)"]
    end

    subgraph Host_Path["Pipeline 107 — ECNS Host Capture Path"]
        MIX105_2 --> MO107["mixout 107.1"]
        MO107 --> HC10["host-copier.10\n(ALSA hw:0,10)"]
    end

    style ECNS fill:#1b4f72,stroke:#555,color:#fff
    style KPB fill:#1c4966,stroke:#555,color:#fff
    style ARB fill:#4a235a,stroke:#555,color:#fff
    style HC10 fill:#1e8449,stroke:#555,color:#fff
    style HC11 fill:#2d5a27,stroke:#555,color:#fff
```

---

## Specifications

| Parameter | Specification |
|---|---|
| Module UUID | `214f8a6c-493a-4b1e-8b1e0d3b6f8a2c15` |
| Processing Domain | DP (Data Processing) thread |
| Execution Period | 20 ms (320 samples at 16 kHz) |
| Input Channels | 4 (Ch 0,1: Microphones; Ch 2,3: Echo Reference) |
| Input Format | 16 kHz · 16-bit signed little-endian (`S16_LE`) |
| Output Pin 0 | 1 channel (Mono clean mic) · 16 kHz · `S16_LE` · OBS = 640 bytes (20ms) |
| Output Pin 1 | 2 channels (Stereo clean mic) · 16 kHz · `S16_LE` · OBS = 1280 bytes (20ms) |
| Sinks Acceptance | Independent sink buffer limit checks (`kpb_frames` vs `host_frames`) |
| Dynamic Pipeline | Compatible (`dynamic_pipeline 1`) |

---

## Verification & Usage

### 1. Recording Clean Stereo Audio (Pin 1 / PCM 10)
```bash
arecord -D hw:0,10 -c 2 -r 16000 -f S16_LE -d 3 /tmp/ecns_stereo.wav
```

### 2. Recording Keyword Detection Pre-Roll Audio (Pin 0 / PCM 11)
```bash
arecord -D hw:0,11 -c 1 -r 16000 -f S16_LE -d 3 /tmp/wov_mono.wav
```

---

## Further Reading
- [Developer Integration Guide: Custom WOV & ECNS Algorithms](../../../doc/developer_guides/wov_ecns_integration_guide.md)


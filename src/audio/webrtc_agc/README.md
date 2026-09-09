# WebRTC Automatic Gain Control Module (`webrtc_agc`)

Wraps the classic WebRTC APM digital AGC (`digital_agc.c`) behind the SOF `module_interface`.

---

## Features

- **Dynamic Speech Volume Leveling**: Automatically boosts quiet speech while holding loud speech at a comfortable target level.
- **Integrated Saturation Limiter**: Hard saturation limiter with smooth decay preventing digital clipping.
- **Pure Fixed-Point Integer**: Operates entirely in integer Q15/Q31 math with no floating-point dependencies (`CONFIG_FPU=n`).
- **Framing FIFO**: Transparently aggregates streaming PCM into 10 ms analysis frames (160 samples @ 16 kHz).
- **Low Compute Overhead**: Consumes **~1.0 – 2.0 MCPS** on an Intel ACE 3.0 (Panther Lake) 400 MHz DSP core (<0.5% DSP load).

---

## Build Configuration

```ini
CONFIG_COMP_WEBRTC_AGC=y
CONFIG_COMP_WEBRTC_AGC_STUB=n
CONFIG_WEBRTC_AGC_TARGET_DBFS=3
CONFIG_WEBRTC_AGC_COMPRESSION_GAIN_DB=9
CONFIG_WEBRTC_AGC_LIMITER=y
```

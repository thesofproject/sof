# WebRTC High-Pass Filter Module (`webrtc_hpf`)

Wraps the WebRTC Audio Processing Module (APM) 2nd-order cascaded biquad High-Pass Filter (HPF) behind the SOF `module_interface`.

---

## Features

- **DC & Sub-Band Rumble Rejection**: Strips DC offsets and mechanical rumble below 80 Hz / 100 Hz.
- **Pure Fixed-Point Integer**: Q12/Q13 filter coefficients with 32-bit accumulators and saturation guards (`CONFIG_FPU=n`).
- **Low Memory Footprint**: Requires only ~32 bytes per channel for state storage (`y[4]`, `x[2]`).
- **Zero Lookahead Latency**: Processes samples sample-by-sample or block-by-block without requiring FIFO framing.
- **Ultra-Low Compute**: Consumes **< 0.3 MCPS** on an Intel ACE 3.0 (Panther Lake) 400 MHz DSP core.

---

## Build Configuration

```ini
CONFIG_COMP_WEBRTC_HPF=y
CONFIG_COMP_WEBRTC_HPF_STUB=n
```

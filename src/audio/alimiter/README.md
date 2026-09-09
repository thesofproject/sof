# FFmpeg Lookahead Peak Limiter (`alimiter`)

Native fixed-point lookahead peak limiter for Sound Open Firmware.

## Overview
- Brickwall peak limiter with smooth lookahead attack window (default 5 ms) and linear release recovery (default 50 ms).
- Protects downstream DACs, amplifiers, and speakers from digital clipping while maximizing perceived loudness.
- Guaranteed output ceiling at 0.950 linear (-0.45 dBFS).
- Pure integer Q31/Q30 arithmetic (zero floating-point dependency, `CONFIG_FPU=n`).
- S16 and S32 format support across up to 4 channels.
- Zero heap allocation during audio streaming; safe for real-time 1 ms timer pipelines on Core 0.

## Benchmark on Panther Lake (ACE 3.0 / HiFi4, 400 MHz)
- Stereo 48 kHz (5 ms lookahead, 50 ms release): **~1.95 MCPS** (<0.50% load).

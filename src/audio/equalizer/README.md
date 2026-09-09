# FFmpeg Parametric Equalizer Module (`equalizer`)

This module ports FFmpeg's `af_biquads` RBJ Audio EQ Cookbook algorithm into a native fixed-point SOF module for Xtensa DSPs.
It applies a 3-band IIR biquad filter cascade (Low shelf, Peaking, High shelf).

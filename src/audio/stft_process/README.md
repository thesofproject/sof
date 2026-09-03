# STFT Process Architecture

This directory provides Short-Time Fourier Transform (STFT) utilities.

## Overview

Converts time-domain audio slices into the frequency domain using FFTs, runs frequency-domain processing over those bins, and applies the inverse-STFT (Overlap-Add) back to the time-domain.

## Configuration and Scripts

- **Kconfig**: Manages the STFT processing component (`COMP_STFT_PROCESS`) and requires multi-channel 32-bit FFT math libraries (`MATH_FFT`, `MATH_32BIT_FFT`, `MATH_FFT_MULTI`). Also optionally supports converting pure FFTs into polar magnitude and phase domains (`STFT_PROCESS_MAGNITUDE_PHASE`).
- **CMakeLists.txt**: Compiles generic STFT operations (`stft_process.c`, `stft_process_common.c`, `stft_process-generic.c`) and IPC4 wrappers (`stft_process-ipc4.c`), fully supporting the `llext` format.
- **stft_process.toml**: Defines the default `STFTPROC` module layout with UUID `UUIDREG_STR_STFT_PROCESS`.
- **Topology (.conf)**: Instantiated from `tools/topology/topology2/include/components/stft_process.conf`, creating a `stft_process` widget type `effect` (UUID `a6:6e:11:0d:50:91:de:46:98:b8:b2:b3:a7:91:da:29`).

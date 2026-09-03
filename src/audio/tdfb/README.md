# Time Domain Filter Bank Architecture

This directory contains the TDFB component.

## Overview

Generally used for operations like beamforming or highly directional microphone array processing in the time domain.

## Configuration and Scripts

- **Kconfig**: Manages the Time Domain Fixed Beamformer component (`COMP_TDFB`), requiring generic `COMP_MODULE_ADAPTER` integration along with high-precision math packages (`MATH_FIR`, `MATH_IIR_DF1`, `SQRT_FIXED`, `CORDIC_FIXED`).
- **CMakeLists.txt**: Builds the standard source set (`tdfb.c`, `tdfb_generic.c`, `tdfb_hifiep.c`, `tdfb_hifi3.c`, `tdfb_direction.c`) handling generic and HIFI implementations alongside IPC abstractions (`tdfb_ipc3.c`, `tdfb_ipc4.c`). Inherits out-of-tree loader (`llext`) generation.
- **tdfb.toml**: Generates ALSA properties mapping the component to `UUIDREG_STR_TDFB`.
- **Topology (.conf)**: Instantiated from `tools/topology/topology2/include/components/tdfb.conf`, creating a `tdfb` widget type `effect` (UUID `49:17:51:dd:fa:d9:5c:45:b3:a7:13:58:56:93:f1:af`). Provides multiple internal mixer controls spanning phase/direction toggles.
- **MATLAB Tuning (`tune/`)**: `sof_example_circular_array.m` and similar array topology scripts define spatial setups (e.g., circular, linear, L-shape, rectangular). They take microphone layout coordinates, compute beamforming angles (azimuth/elevation) and delays, and pack them into binary configuration blobs allowing microphones to "steer" their listening direction interactively.

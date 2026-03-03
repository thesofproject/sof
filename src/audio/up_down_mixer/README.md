# Up/Down Mixer Architecture

This directory contains the Up/Down channel mixer.

## Overview

Converts the spatial format of audio (e.g., Stereo up to 5.1 surround, or 5.1 mixed down to Stereo) natively.

## Configuration and Scripts

- **Kconfig**: Enabled via `COMP_UP_DOWN_MIXER`, specifically tuned for `IPC_MAJOR_4`. Supports heavy format transformations up to 7.1 endpoints.
- **CMakeLists.txt**: Implements the base `up_down_mixer.c` component along with highly-optimized architecture primitives (`up_down_mixer_hifi3.c`).
- **up_down_mixer.toml**: Robust configuration map controlling `UPDWMIX` deployment against variable system arrays depending on Meteor Lake, Lunar Lake, or ACE platform parameters (UUID binding `UUIDREG_STR_UP_DOWN_MIXER`).

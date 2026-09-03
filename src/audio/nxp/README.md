# NXP Specific Components Architecture

## Overview

This directory contains DSP components tailored for NXP hardware acceleration or special features.

## Configuration and Scripts

- **Kconfig**: Manages the proprietary NXP Essential Audio Processing blocks (`COMP_NXP_EAP`), aiming to enhance tonal and spatial audio perception. Also supports testing through a stub mechanism (`COMP_NXP_EAP_STUB`).
- **CMakeLists.txt**: Configures external NXP SDK directories and intelligently links the `eap.c` wrapper either against the stub logic or the authentic `libEAP16_3_0_13_FP1_RT600.a` static library blob.

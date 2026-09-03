# Tone Generator Architecture

This directory contains the Tone component.

## Overview

Synthesizes simple sine-waves or beep sequences, generally used for internal testing, diagnostics, or system alert sounds.

## Configuration and Scripts

- **Kconfig**: Controls compilation inclusion via `COMP_TONE` acting securely on top of the generic `COMP_MODULE_ADAPTER` and `IPC_MAJOR_4`. Depends on the `CORDIC_FIXED` library for sine approximations.
- **CMakeLists.txt**: Links `tone.c` to respective system-level pipelines (`tone-ipc3.c` or `tone-ipc4.c`) and allows execution seamlessly as a zephyr `llext`.
- **tone.toml**: Describes module structure mappings assigning `UUIDREG_STR_TONE` up to multi-instance capacities.

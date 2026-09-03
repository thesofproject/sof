# Level Multiplier Architecture

This directory contains the Level Multiplier component.

## Overview

Applies simple digital gain/attenuation (multiplication) to an audio signal stream.

## Configuration and Scripts

- **Kconfig**: Enables the Level multiplier component (`COMP_LEVEL_MULTIPLIER`). This applies a configured fixed gain to an audio stream, typically to increase capture sensitivity of voice applications compared to media capture.
- **CMakeLists.txt**: Manages local base sources and optimized implementations (`level_multiplier-generic.c`, `level_multiplier-hifi3.c`, `level_multiplier-hifi5.c`). Includes the IPC version 4 wrapper (`level_multiplier-ipc4.c`) when `CONFIG_IPC_MAJOR_4` is present, and supports `llext`.
- **level_multiplier.toml**: Holds topology module entry parameters including UUIDs, standard pin layouts, and `mod_cfg` limits.
- **Topology (.conf)**: `tools/topology/topology2/include/components/level_multiplier.conf` configures the `level_multiplier` widget object. It specifies `num_input_pins` and `num_output_pins` mapping and defaults to type `effect` with UUID `56:74:39:30:61:46:44:46:97:e5:39:a9:e5:ab:17:78`.

# Sound Dose Architecture

This directory contains the Sound Dose component.

## Overview

Monitors the acoustic energy output to headphones over time, ensuring it complies with regulations governing maximum cumulative sound exposure (to protect user hearing).

## Configuration and Scripts

- **Kconfig**: Dictates the Sound Dose component (`COMP_SOUND_DOSE`) with prerequisites on `IPC_MAJOR_4` and `COMP_MODULE_ADAPTER`, plus mathematical modules (`MATH_EXP`, `MATH_IIR`).
- **CMakeLists.txt**: Compiles generic operations (`sound_dose-generic.c`, `sound_dose.c`) alongside their IPC wrappers (`sound_dose-ipc4.c`), offering full `llext` enablement.
- **sound_dose.toml**: Contains topology descriptors, declaring UUID `UUIDREG_STR_SOUND_DOSE` and standard memory allocation bounds.
- **Topology (.conf)**: Extracted from `tools/topology/topology2/include/components/sound_dose.conf`, configuring a `sound_dose` widget of type `effect` (UUID `7c:9d:3f:a4:75:ea:d5:44:94:2d:96:79:91:a3:38:09`).

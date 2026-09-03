# Channel Selector Architecture

This directory contains the Channel Selector component.

## Overview

Given a multi-channel stream (e.g., 8-channel microphone array), the channel selector component isolates specific channels (e.g., channels 1 and 2) to pass forward while dropping the rest.

## Configuration and Scripts

- **Kconfig**: Enables the selector component (`COMP_SEL`).
- **CMakeLists.txt**: Compiles `selector.c` and `selector_generic.c`, and natively supports Zephyr environment modules (`llext`).
- **selector.toml**: Includes topology parameters under `MICSEL`, using UUID `UUIDREG_STR_SELECTOR4` and limits up to 8 instances, with distinct `mod_cfg` limits depending on chipset architectures (Meteor Lake, Lunar Lake, ACE).
- **Topology (.conf)**: Derived from `tools/topology/topology2/include/components/micsel.conf`, which defines a `micsel` widget of type `effect` (UUID `c1:92:fe:32:17:1e:c2:4f:97:58:c7:f3:54:2e:98:0a`).

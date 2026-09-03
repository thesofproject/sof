# Template Component Architecture

This directory contains template code.

## Overview

Provides a reference or stub implementation that developers can copy and modify to easily construct new custom audio components.

## Configuration and Scripts

- **Kconfig**: Exposes the `COMP_TEMPLATE` toggle to build the baseline framework plugin (internally swaps or reverses channels).
- **CMakeLists.txt**: Compiles generic operations (`template.c`, `template-generic.c`) and connects via either IPC major version. Generates a Zephyr `llext` by default.
- **template.toml**: Includes default setup arguments and UUID bindings (`UUIDREG_STR_TEMPLATE`).
- **Topology (.conf)**: Defined in `tools/topology/topology2/include/components/template_comp.conf`, registering the `template_comp` widget of type `effect` (UUID `af:e1:2d:a6:64:59:2e:4e:b1:67:7f:dc:97:27:9a:29`).

# Intelligo Noise Reduction (IGO NR) Architecture

This directory contains integration for the Intelligo Noise Reduction algorithms.

## Overview

IGO NR is an advanced noise suppression engine targeting microphone input paths to improve speech intelligibility.

## Configuration and Scripts

- **Kconfig**: Enables the Intelligo non-speech noise reduction component (`COMP_IGO_NR`), which utilizes a proprietary binary `libigonr.a`. Also provides a `COMP_IGO_NR_STUB` option for testing/CI environments that links a stub library instead of the proprietary binary.
- **CMakeLists.txt**: Handles the build integration. It either links the stub (`igo_nr_stub.c`) or the actual binary (`libigonr.a`) based on the Kconfig selection. Supports Zephyr integration module loading and standard external library compilation (`llext`).
- **igo_nr.toml**: Defines the topology parameters for the module (UUID, module_type, pin configuration, and processing constraints configured under `mod_cfg`).
- **Topology (.conf)**: Derived from `tools/topology/topology2/include/components/igo_nr.conf`, it defines the `igo_nr` widget object for topology generation. Included with mixer controls binding get/put operations (`259`) and defaults to UUID `bc:e2:6a:69:77:28:eb:11:ad:c1:02:42:ac:12:00:02` (type `effect`).

# Codec Abstraction Architecture

This directory contains abstractions and adapters for various hardware and software audio codecs.

## Overview

Provides a unified interface to initialize, configure, and stream data to/from codec dependencies.

## Configuration and Scripts

- **Kconfig**: Specifically manages configurations for external codecs. For example, it defines options for the `DTS_CODEC`, including a testing/CI stub `DTS_CODEC_STUB` when `COMP_STUBS` is enabled. Use of the actual DTS codec requires a pre-compiled static library from Xperi.
- **CMakeLists.txt**: Specifies Zephyr build integration. For the DTS codec, it checks for modular builds (`llext`). If built statically, it either links the stub source or imports the pre-compiled `libdts-sof-interface-i32.a` library depending on configuration.
- **Topology (.conf)**: `tools/topology/topology2/include/components/dts.conf` defines the `dts` widget. It exposes `cpc` (cycles per chunk) and configures a byte control of size 2048 with `extctl` operations. Defaults to UUID `4f:c3:5f:d9:0f:37:c7:4a:bc:86:bf:dc:5b:e2:41:e6`.

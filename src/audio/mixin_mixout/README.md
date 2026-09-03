# Mix-In / Mix-Out Architecture

This directory provides specialized Mix-in and Mix-out instances.

## Overview

These components typically act as routing endpoints to inject or extract specific streams in/out of an ongoing audio mix.

## Configuration and Scripts

- **Kconfig**: Compiles the Mixin_mixout component (`COMP_MIXIN_MIXOUT`), which depends on the modern `IPC_MAJOR_4`. Allows choosing SIMD optimization logic explicitly.
- **CMakeLists.txt**: Integrates generic, HIFI3, and HIFI5 specialized processing source files. Fully supports modular execution via `llext`.
- **mixin_mixout.toml**: Extensive configuration separating `MIXIN` and `MIXOUT` instances. Configures UUIDs, domain types, and highly localized `mod_cfg` arrays adapted for `CONFIG_METEORLAKE`, `CONFIG_LUNARLAKE`, and ACE SOCs.
- **Topology (.conf)**: Uses `tools/topology/topology2/include/components/mixin.conf` (type `mixer` with `mix_type` `"mix_in"`, defaulting to UUID `b2:6e:65:39:71:3b:49:40:8d:3f:f9:2c:d5:c4:3c:09`, supporting 3 output pins) and `mixout.conf` (type `mixer` with `mix_type` `"mix_out"`, defaulting to UUID `5a:50:56:3c:d7:24:8f:41:bd:dc:c1:f5:a3:ac:2a:e0`, mapping to 8 input pins). Both force a 32-bit depth processing format.

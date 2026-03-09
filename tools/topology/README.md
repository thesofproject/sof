# SOF Topology Tools (`tools/topology`)

The `tools/topology` directory contains the build infrastructure and source files used to generate Advanced Linux Sound Architecture (ALSA) topology (`.tplg`) binaries for Sound Open Firmware.

ALSA topology files describe the audio DSP graph (pipelines, widgets, routes, DAIs) and are loaded dynamically by the Linux kernel SOF driver during initialization. This allows a single generic driver to support a wide variety of hardware configurations and routing paths without requiring source code changes in the kernel.

## Directory Structure

To support the evolution of the SOF topology syntax, the configuration files are split into two main versions:

- [`topology1/`](topology1/README.md): Contains the legacy (v1) `m4`-based ALSA topology generation scripts and configuration files. Topology v1 heavily relies on `m4` macro preprocessing to generate the final `.conf` files before being compiled by `alsatplg`.
- [`topology2/`](topology2/README.md): Contains the newer (v2) ALSA topology generation files. Topology v2 introduces a more structured, object-oriented syntax natively supported by newer versions of the `alsatplg` compiler (specifically the pre-processor `-p` flag), reducing reliance on external macro languages.

## Build Process

The topology build process is managed by `CMakeLists.txt` in this root directory. It performs the following key steps:

1. **Compiler Detection**: It locates the `alsatplg` tool (usually built in `tools/bin/alsatplg` alongside the firmware) and checks its version.
2. **Feature Validation**: It ensures the `alsatplg` version is at least `1.2.5`. Older versions are known to silently corrupt certain topology structures (e.g., converting `codec_consumer` to `codec_master`). If the tool is too old, topology generation is safely skipped with a warning.
3. **Target Generation**: It provides macros (`add_alsatplg_command` and `add_alsatplg2_command`) used by the subdirectories to invoke the topology compiler on the pre-processed `.conf` files to generate the final `.tplg` binaries.

The `topologies` CMake target is the master target that depends on the generation targets inside both `topology1` and `topology2`.

# SOF Helper Scripts

This folder contains a lot of useful scripts that can speed up development for routine tasks or simplify execution of complex tasks.

## Build Scripts

SOF has several build targets depending on whether you are building firmware, tooling, documentation or topologies. This directory has a helper for each.

### Firmware Build (`xtensa-build-zephyr.py`)

Firmware can either be built using west commands directly or by the `xtensa-build-zephyr.py` script. This script wraps up the west commands and can build using either the Zephyr SDK compiler or the Cadence xtensa compiler for xtensa targets.

Please run the script with `--help` to see all options.

E.g to build SOF for Intel Pantherlake:

1) Enable the python virtual environment for west. This should be in your SOF workspace installation directory. Default is `~/work/sof` (only needs run once).

```bash
source ~/work/sof/.venv/bin/activate
```

2) Now run the build script. *Note: most build errors are a result of ingredients being out of sync with the west manifest. Please run `west update` and rebuild before fixing/reporting build errors.*

```bash
./scripts/xtensa-build-zephyr.py -p ptl
```

### Reproducible Output Builds (`test-repro-build.sh`)

This script can be used to locally reproduce the exact build steps and environment of specific CI validation tests.

```bash
./scripts/test-repro-build.sh --help
```

## Tools and Topologies

Tooling and topology can be built together using one script. To build all topologies please run:

```bash
./scripts/build-tools.sh
```

**Options for `build-tools.sh`:**

* `-c` : Rebuild `ctl/` tool
* `-l` : Rebuild `logger/` tool
* `-p` : Rebuild `probes/` tool
* `-T` : Rebuild ALL `topology/` targets
* `-X` : Rebuild topology1 only
* `-Y` : Rebuild topology2 only
* `-t` : Rebuild test topologies
* `-A` : Clone and rebuild the local ALSA git version for `alsa-lib` and `alsa-utils` with latest non-distro features.
* `-C` : No build, only CMake re-configuration. Shows CMake targets.

*Warning: building tools is incremental by default. To build from scratch delete the `tools/build_tools` directory or use `-C`.*

### ALSA Specific Build (`build-alsa-tools.sh`)

If you want to pull down and explicitly recompile only the associated ALSA libraries from their public `alsa-lib` GitHub upstream independently of SOF topologies:

```bash
./scripts/build-alsa-tools.sh
```

## Testbench and Emulation

Testbench is a host application that is used to run SOF processing modules on developers PC. This allows for module development using regular host based tooling.

### Rebuilding the Testbench (`rebuild-testbench.sh`)

This script cleans and rebuilds the host test application binary. Ensure you supply the correct target platform wrapper or fuzzing backend.

**Usage Options:**

* `-p <platform>` : Build testbench binary for `xt-run` for selected platform (e.g. `-p tgl`). When omitted, performs a `BUILD_TYPE=native`, compile-only check.
* `-f` : Build testbench via a compiler provided by a fuzzer (default path: `.../AFL/afl-gcc`).
* `-j` : Number of parallel make jobs (defaults to `nproc`).

### Running the Testbench (`host-testbench.sh`)

Starts the testing sequences. This invokes specific components to ensure basic inputs process without segfaults.

```bash
./scripts/host-testbench.sh
```

### QEMU Check (`qemu-check.sh`)

Automated verifier for executing firmware builds under QEMU emulation.

**Usage:**

```bash
./scripts/qemu-check.sh [platform(s)]
```

* Supported platforms are: `imx8`, `imx8x`, `imx8m`.
* Runs all supported platforms by default if none are provided.

## SDK Support

There is some SDK support in this directory for speeding up or simplifying tasks with multiple steps.

### New Modules (`sdk-create-module.py`)

A new module can be created by running the SDK Create Module script. This python helper copies the SOF template audio module and renames all strings, Cmakefiles, and Kconfigs automatically. It also correctly registers a new DSP UUID and TOML entries.

Please run:

```bash
./scripts/sdk-create-module.py new_module_name
```

## Docker

The docker container provided in `docker_build` sets up a build environment for building Sound Open Firmware. A working docker installation is needed to run the docker build container.

*Note: In order to run docker as non sudo/root user please run:*

```bash
sudo usermod -aG docker your-user-name
```

Then logout and login again.

**Quick Start:**

First, build the docker container. This step needs to be done initially and when the toolchain or ALSA dependencies are updated.

```bash
cd scripts/docker_build
./docker-build.sh
```

After the container is built, it can be used to run the scripts.

To build for tigerlake:

```bash
./scripts/docker-run.sh ./scripts/xtensa-build-all.sh -l tgl
```

or (this command may prompt for a password during rimage installation inside the container)

```bash
./scripts/docker-run.sh ./scripts/xtensa-build-all.sh tgl
```

To rebuild the topology and logger:

```bash
./scripts/docker-run.sh ./scripts/build-tools.sh
```

An incremental `sof.git` build:

```bash
./scripts/docker-run.sh make
```

Or enter a shell:

```bash
./scripts/docker-run.sh bash
```

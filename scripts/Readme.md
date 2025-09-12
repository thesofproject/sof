# SOF Helper Scripts

This folder contains a lot of useful scripts that can speed up development for routine tasks or simplify execution of complex tasks.

## Build Scripts

SOF has several build targets depending on whether you are building firmware, tooling, documentation or topologies. This directory has a helper for each.

### Firmware

Firmware can either be built using west command directly or by the xtensa-build-zephyr.py script. This script wraps up the west commands and can build using either the Zephyr SDK compiler or the Cadence xtensa compiler for xtensa targets.

Please run the script with --help to see all options.

E.g to build SOF for Intel Pantherlake:

1) Enable the python virtual environment for west. This should be in your SOF workspace installation direction. Default is ```~/work/sof``` (only needs run once).
```bash
source ~/work/sof/.venv/bin/activate
```
2) Now run the build script. *Note: most build errors are a result of ingredients being out of sync with the west manifest. Please run ```west update``` and rebuild before fixing/reporting build errors.*
```bash
./scripts/xtensa-build-zephyr.py -p ptl
```

### Testbench

Testbench is a host application that is used to run SOF processing modules on developers PC. This allows for module development using regular host based tooling.

Please run
```bash
./rebuild-testbench.sh --help
```
for full options.

Testbench can be also be built for Cadence simulator targets.

### Tools and Topologies

Tooling and topology can be built together using one script. To build all topologies please run:

```bash
./scripts/build-tools.sh
```

This script can build:
1) sof-ctl
2) sof-logger
3) probes
4) all topology 1 & 2 and test topologies.
5) Local ALSA git version for alsa-lib and alsa-utils that have features not yet in distro version of ALSA packages.

## SDK Support

There is some SDK support in this directory for speeding up or simplifying tasks with multiple steps.

### New Modules

A new module can be created by running the sdk-create-module script. This script will copy the template module and rename all strings, Cmakefiles, Kconfigs to match the new module. It will also create a UUID for the new module and a TOML manifest entry (for targets that need this).

Please run
```bash
./sdk-create-module.py new_module_name
```

## Docker

The docker container provided in docker_build sets up a build environment for
building Sound Open Firmware. A working docker installation is needed to run
the docker build container.

Note: In order to run docker as non sudo/root user please run.
```bash
sudo usermod -aG docker your-user-name
```
Then logout and login again.

Quick Start:

First, build the docker container. This step needs to be done initially and
when the toolchain or alsa dependencies are updated.
```bash
cd scripts/docker_build
./docker-build.sh
```
After the container is built, it can be used to run the scripts.

To build for tigerlake:
```bash
./scripts/docker-run.sh ./scripts/xtensa-build-all.sh -l tgl
```
or (may need password test0000 for rimage install)
```bash
./scripts/docker-run.sh ./scripts/xtensa-build-all.sh tgl
```
To rebuild the topology and logger:
```bash
./scripts/docker-run.sh ./scripts/build-tools.sh
```
An incremental sof.git build:
```bash
./scripts/docker-run.sh make
```
Or enter a shell:
```bash
./scripts/docker-run.sh bash
```

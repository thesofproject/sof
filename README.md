# Sound Open Firmware

### Status
[![Build Status](https://travis-ci.org/thesofproject/sof.svg?branch=master)](https://travis-ci.org/thesofproject/sof)

### Documentation

See [docs](https://thesofproject.github.io/latest/index.html)

### Prerequisites

* Docker
* CMake (version >= 3.10)

### Build Instructions

1. Create directory in checked out repo for build files:
```
mkdir build && cd build
```

2. Run configuration for your toolchain:

Baytrail:

```
cmake -DTOOLCHAIN=xtensa-byt-elf -DROOT_DIR=`pwd`/../../xtensa-root/xtensa-byt-elf ..
```

Cherrytrail:

```
cmake -DTOOLCHAIN=xtensa-cht-elf -DROOT_DIR=`pwd`/../../xtensa-root/xtensa-cht-elf ..
```

Haswell / Broadwell:

```
cmake -DTOOLCHAIN=xtensa-hsw-elf -DROOT_DIR=`pwd`/../../xtensa-root/xtensa-hsw-elf ..
```

Apollolake:

```
cmake -DTOOLCHAIN=xtensa-apl-elf -DROOT_DIR=`pwd`/../../xtensa-root/xtensa-apl-elf ..
```

Cannonlake:

```
cmake -DTOOLCHAIN=xtensa-cnl-elf -DROOT_DIR=`pwd`/../../xtensa-root/xtensa-cnl-elf ..
```

3. Apply default config for your platform.

Baytrail:

```
make baytrail_defconfig
```

Cherrytrail:

```
make cherrytrail_defconfig
```

Haswell:

```
make haswell_defconfig
```

Broadwell:

```
make broadwell_defconfig
```

Apollolake:

```
make apollolake_defconfig
```

Cannonlake:

```
make cannonlake_defconfig
```

4. (Optional) Customize your configuration

```
make menuconfig
```

5. Build firmware

```
make bin
# or `make bin -j<jobs>` for parallel build
```

## Running the tests

See [unit testing documentation](https://thesofproject.github.io/latest/developer_guides/unit_tests.html)

## Deployment

TODO: Add additional notes about how to deploy this on a live system

## Contributing

See [Contributing to the Project](https://thesofproject.github.io/latest/contribute/index.html)

## License

This project is licensed under the BSD Clause 3 - see the [LICENCE](LICENCE) file for details

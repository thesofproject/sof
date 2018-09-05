# Sound Open Firmware

### Status
[![Build Status](https://travis-ci.org/thesofproject/sof.svg?branch=master)](https://travis-ci.org/thesofproject/sof)

### Documentation

See [docs](https://thesofproject.github.io/latest/index.html)

### Prerequisites

* Docker

### Build Instructions

1. Run `autogen.sh`

2. Build and install the rimage ELF image creator and signing tool

```
./configure --enable-rimage
make
sudo make install
```

3. Run the following configure based on your platform.

Baytrail :-

```./configure --with-arch=xtensa --with-platform=baytrail --with-root-dir=$PWD/../xtensa-root/xtensa-byt-elf --host=xtensa-byt-elf```

Cherrytrail :-

```./configure --with-arch=xtensa --with-platform=cherrytrail --with-root-dir=$PWD/../xtensa-root/xtensa-byt-elf --host=xtensa-byt-elf```

Library for Host Platform :-
If building library for host platform, run the following configure. Please modify
the --prefix option to choose the directory for installing the library files and
headers

`./configure --with-arch=host --enable-library=yes --host=x86_64-unknown-linux-gnu --prefix=$PWD/../host-root/`

4. `make`

5. `make bin`

End with an example of getting some data out of the system or using it for a little demo

## Running the tests

TODO

## Deployment

Add additional notes about how to deploy this on a live system

## Contributing

TODO?

## License

This project is licensed under the BSD Clause 3 - see the [LICENCE](LICENCE) file for details

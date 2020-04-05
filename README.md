# rimage

`rimage` is a DSP firmware image creation and signing tool targeting
the DSP on certain Intel System-on-Chip (SoC). This is used by
the [Sound Open Firmware (SOF)](https://github.com/thesofproject/sof)
to generate binary image files.

## Building

The `rimage` tool can be built with the following commands:

```shell
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make install
```

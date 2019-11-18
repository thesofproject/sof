# rimage

`rimage` is a DSP firmware image creation and signing tool targeting
the DSP on certain Intel System-on-Chip (SoC). This is used by
the [Sound Open Firmware (SOF)](https://github.com/thesofproject/sof)
to generate binary image files.

## Building

The `rimage` tool can be built with or without logging dictionary
support, where such support is required to decipher the log messages
produced the the SOF firmware.

#### Build without dictionary support:

```shell
$ ./configure
$ make
$ make install
```

#### Build with dictionary support:

To build `rimage` with dictionary support, the SOF source is required.
First, clone the SOF source tree from
[here](https://github.com/thesofproject/sof). Then, do:

```shell
$ ./configure --with-sof-source=<full path to SOF source>
$ make
$ make install
```

Note that creating the SOF firmware image requires dicitionary support.

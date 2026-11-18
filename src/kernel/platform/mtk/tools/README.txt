
MTK AFE Generator tool
======================

Early versions of the AFE driver were published with large C struct
definitions tied to platform headers.  Zephyr strongly prefers
Devicetree for configuration and not C code.

So this is a somewhat klugey C program that builds and runs the AFE
platform code on a Linux host CPU, producing valid DTS output that can
be included in a board devicetree file in Zephyr.

Just run ./build.sh from inside this directory.  The only required
host software is a working gcc that supports the "-m32" flag.

The resulting afe-<platform>.dts files can be included directly in
Zephyr board config.

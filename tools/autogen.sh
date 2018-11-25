#!/bin/bash
libtoolize -c --force
aclocal -I m4 --install
autoconf -Wall
autoheader
automake -a --copy --foreign --add-missing

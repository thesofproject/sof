#!/bin/bash
rm -f .build
libtoolize -c --force
aclocal -I m4 --install
autoconf -Wall
autoheader
automake -a --copy --foreign --add-missing

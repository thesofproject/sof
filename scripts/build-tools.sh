#!/bin/sh
cd tools
./autogen.sh
./configure
make -j$(nproc)
cd ../

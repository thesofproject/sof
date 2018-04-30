#!/bin/sh
cd ../soft.git
./autogen.sh
./configure
make -j$(nproc)
cd ../sof.git

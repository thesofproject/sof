#!/bin/sh
sudo mkdir -p /usr/local/include/sof/uapi
sudo cp src/include/uapi/logging.h /usr/local/include/sof/uapi/logging.h
cd ../soft.git
./autogen.sh
./configure
make -j$(nproc)
cd ../sof.git

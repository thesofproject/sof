#!/bin/bash

# fail on any errors
set -e

mkdir build_host
cd build_host
cmake -DBUILD_HOST=ON -DCMAKE_INSTALL_PREFIX=install ..
make -j4
make install

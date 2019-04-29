#!/bin/bash

# fail on any errors
set -e

cd tools
cd testbench

rm -rf build_testbench

mkdir build_testbench
cd build_testbench

cmake -DCMAKE_INSTALL_PREFIX=install \
	-DCMAKE_VERBOSE_MAKEFILE=ON \
	..

make -j$(nproc --all)
make install

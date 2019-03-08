#!/bin/bash

# fail on any errors
set -e

rm -rf build_host

mkdir build_host
cd build_host

cmake -DBUILD_HOST=ON \
	-DCMAKE_INSTALL_PREFIX=install \
	-DCMAKE_VERBOSE_MAKEFILE=ON \
	..

make -j4
make install

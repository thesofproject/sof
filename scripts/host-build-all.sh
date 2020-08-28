#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# fail on any errors
set -e

deprecated()
{
    >&2 printf "\nWARNING: %s is deprecated, use newer rebuild-testbench.sh\n\n" \
	"$0"
}

deprecated

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

deprecated

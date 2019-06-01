#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# fail immediately on any errors
set -e

cd tools

rm -rf build_tools

mkdir build_tools
cd build_tools
cmake ..
make -j$(nproc)
if [[ "$1" == "-t" ]]; then
	make tests -j$(nproc)
fi
cd ../../

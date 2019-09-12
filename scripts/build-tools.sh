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
for args in $@
do
	#build test topologies
	if [[ "$args" == "-t" ]]; then
		make tests -j$(nproc)
	fi
	#build fuzzer
	if [[ "$args" == "-f" ]]; then
		rm -rf build_fuzzer
		mkdir build_fuzzer
		cd build_fuzzer
		cmake ../../fuzzer
		make -j$(nproc)
		cd ../
	fi
done

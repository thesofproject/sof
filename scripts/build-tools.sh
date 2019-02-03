#!/bin/bash
cd tools

mkdir build_tools
cd build_tools
cmake ..
make -j$(nproc)
if [[ "$1" == "-t" ]]; then
	make tests -j$(nproc)
fi
cd ../../

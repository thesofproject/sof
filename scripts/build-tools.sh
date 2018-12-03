#!/bin/bash
cd tools
./autogen.sh
./configure
make -j$(nproc)
if [[ "$1" == "-t" ]]; then
	make tests -j$(nproc)
fi
cd ../

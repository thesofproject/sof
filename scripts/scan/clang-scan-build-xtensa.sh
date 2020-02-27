#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation. All rights reserved.
# Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

# This script configures & performs build for clang's static analyzer.

set -eu

function usage {
	echo "Usage: $0 -t <toolchain> -c <config> -r <root_dir> [options]"
	echo "        -t        Toolchain's name."
	echo "        -c        Name of defconfig."
	echo "        -r        Xtensa root dir."
	echo "        [-j n]    Set number of make build jobs."
	echo "        [-v]      Verbose output."
	echo "Examples:"
	echo "        $0 -t xt -c apollolake \\"
	echo "                -r \$CONFIG_PATH/xtensa-elf"
	echo "        $0 -t xtensa-cnl-elf -c cannonlake \\"
	echo "                -r \`pwd\`/../xtensa-cnl-elf"
}

if [ "$#" -eq 0 ]
then
	usage
	exit 1
fi

TOOLCHAIN=""
DEFCONFIG=""
ROOT_DIR=""
JOBS=$(nproc --all)
VERBOSE_CMAKE=OFF
VERBOSE_SCAN_BUILD=""

while getopts "t:c:r:j:v" OPTION; do
	case $OPTION in
		t)
			TOOLCHAIN=$OPTARG
			;;
		c)
			DEFCONFIG=$OPTARG
			;;
		r)
			ROOT_DIR=$OPTARG
			;;
		j)
			JOBS=$OPTARG
			;;
		v)
			VERBOSE_CMAKE=ON
			VERBOSE_SCAN_BUILD="-v -v"
			;;
		*)
			echo "Invalid options"
			usage
			exit 1
			;;
	esac
done

WORKDIR=`pwd`
SCRIPT_DIR=$(realpath $(dirname $0))
PROXY_COMPILER=$SCRIPT_DIR/proxy-gcc.sh

BUILD_DIR=build_clang_scan_${DEFCONFIG}_${TOOLCHAIN}

rm -fr $BUILD_DIR
mkdir $BUILD_DIR
cd $BUILD_DIR

scan-build $VERBOSE_SCAN_BUILD cmake \
	-DBUILD_CLANG_SCAN=ON \
	-DTOOLCHAIN=$TOOLCHAIN \
	-DROOT_DIR=$ROOT_DIR \
	-DCMAKE_VERBOSE_MAKEFILE=$VERBOSE_CMAKE \
	..

make ${DEFCONFIG}_defconfig

if [[ "$TOOLCHAIN" == "xt" ]]
then
	COMPILER=$TOOLCHAIN-xcc
else
	COMPILER=$TOOLCHAIN-gcc
fi

# There are flags that are needed by clang but don't work for xtensa compiler.
# However clang tries to pass all flags to original compiler, so we need to put
# proxy compiler in the middle that is going to filter out unsupported flags.
PROXY_CC=$COMPILER PROXY_REMOVE_FLAGS=-m32 \
	scan-build $VERBOSE_SCAN_BUILD --use-cc="$PROXY_COMPILER" -o report \
	make sof -j$JOBS

cd "$WORKDIR"

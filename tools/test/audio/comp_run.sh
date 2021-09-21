#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2020 Intel Corporation. All rights reserved.

# stop on most errors
set -e

COMP=$1
DIRECTION=$2
BITS_IN=$3
BITS_OUT=$4
FS1=$5
FS2=$6
FN_IN=$7
FN_OUT=$8

# Paths
HOST_ROOT=../../testbench/build_testbench
HOST_EXE=$HOST_ROOT/install/bin/testbench
HOST_LIB=$HOST_ROOT/sof_ep/install/lib
TPLG_LIB=$HOST_ROOT/sof_parser/install/lib
TPLG_DIR=../../build_tools/test/topology

# Use topology from component test topologies
INFMT=s${BITS_IN}le
OUTFMT=s${BITS_OUT}le
TPLGFN=test-${DIRECTION}-ssp5-mclk-0-I2S-${COMP}-${INFMT}-${OUTFMT}-48k-24576k-codec.tplg
TPLG=${TPLG_DIR}/${TPLGFN}

# If binary test vectors
if [ "${FN_IN: -4}" == ".raw" ]; then
    BINFMT="-b S${BITS_IN}_LE"
else
    BINFMT=""
fi

# Run command
# There is no more need to specify the library explicitly
#LIB="-a ${COMP}=libsof_${COMP}.so"
LIB=""
ARG="-d -r $FS1 -R $FS2 -i $FN_IN -o $FN_OUT -t $TPLG $BINFMT $LIB"
CMD="$HOST_EXE $ARG"
export LD_LIBRARY_PATH=$HOST_LIB:$TPLG_LIB

# Run test bench
echo "Command:         $HOST_EXE"
echo "Argument:        $ARG"
echo "LD_LIBRARY_PATH: ${LD_LIBRARY_PATH}"

valgrind --leak-check=yes --error-exitcode=1 $CMD

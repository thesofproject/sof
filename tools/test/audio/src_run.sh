#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

COMP=src
TEST=playback

BITS_IN=$1
BITS_OUT=$2
FS1=$3
FS2=$4
FN_IN=$5
FN_OUT=$6

# Paths
HOST_ROOT=../../testbench/build_testbench
HOST_EXE=$HOST_ROOT/install/bin/testbench
HOST_LIB=$HOST_ROOT/sof_ep/install/lib
TPLG_LIB=$HOST_ROOT/sof_parser/install/lib
TPLG_DIR=../../test/topology

# Use topology from component test topologies
INFMT=s${BITS_IN}le
OUTFMT=s${BITS_OUT}le
TPLGFN=test-${TEST}-ssp5-mclk-0-I2S-${COMP}-${INFMT}-${OUTFMT}-48k-24576k-codec.tplg
TPLG=${TPLG_DIR}/${TPLGFN}

# If binary test vectors
if [ ${FN_IN: -4} == ".raw" ]; then
    BINFMT="-b S${BITS_IN}_LE"
else
    BINFMT=""
fi

# Run command
LIB="-a ${COMP}=libsof_${COMP}.so"
ARG="-d -r $FS1 -R $FS2 -i $FN_IN -o $FN_OUT -t $TPLG $BINFMT $LIB"
CMD="$HOST_EXE $ARG"
export LD_LIBRARY_PATH=$HOST_LIB:$TPLG_LIB

# Run test bench
echo "Command:         $HOST_EXE"
echo "Argument:        $ARG"
echo "LD_LIBRARY_PATH: ${LD_LIBRARY_PATH}"

$CMD

#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# SRC specifics
COMP=src
BITS_IN=$1
BITS_OUT=$2
FS1=$3
FS2=$4
FN_IN=$5
FN_OUT=$6

# The HOST_ROOT path need to be retrived from SOFT .configure command
HOST_ROOT=../../testbench/build_testbench
HOST_EXE=$HOST_ROOT/install/bin/testbench
HOST_LIB=$HOST_ROOT/sof_ep/install/lib
TPLG_LIB=$HOST_ROOT/sof_parser/install/lib
export LD_LIBRARY_PATH=$HOST_LIB:$TPLG_LIB

# Use topology from component test topologies
INFMT=s${BITS_IN}le
OUTFMT=s${BITS_IN}le
MCLK=24576k
tplg1=../../test/topology/test-playback-ssp2-mclk-0-I2S-
tplg2=${COMP}-${INFMT}-${OUTFMT}-48k-${MCLK}-nocodec.tplg
TPLG=${tplg1}${tplg2}

# Alternatively use platform specific topologies
# though currently test works only with s24 data due
# to not fully set volume component in the pipeline.
#TPLG=../../build_tools/topology/sof-apl-src-pcm512x.tplg

# If binary test vectors
if [ ${FN_IN: -4} == ".raw" ]; then
    CMDFMT="-b S${BITS_IN}_LE"
else
    CMDFMT=""
fi

# Run command
CMD0=$HOST_EXE
arg1="-d -r $FS1 -R $FS2 -i $FN_IN -o $FN_OUT -t $TPLG"
arg2="-a src=libsof_${COMP}.so $CMDFMT"
CMD1="$arg1 $arg2"
CMD="$CMD0 $CMD1"

# Run test bench
echo "Command:     $CMD0"
echo "Arg:         $CMD1"
echo "Ld lib path: $LD_LIBRARY_PATH"
$CMD

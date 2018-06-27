#!/bin/bash

# SRC specifics
COMP=src
BITS_IN=$1
BITS_OUT=$2
FS1=$3
FS2=$4
FN_IN=$5
FN_OUT=$6

# The HOST_ROOT path need to be retrived from SOFT .configure command
HOST_ROOT=../../host-root
HOST_EXE=$HOST_ROOT/bin/testbench
HOST_LIB=$HOST_ROOT/lib

# Topology
INFMT=s${BITS_IN}le
OUTFMT=s${BITS_IN}le
MCLK=24576k
TPLG=../topology/test/test-playback-ssp2-I2S-${COMP}-${INFMT}-${OUTFMT}-48k-${MCLK}-nocodec.tplg

# If binary test vectors
if [ ${FN_IN: -4} == ".raw" ]; then
    CMDFMT="-b S${BITS_IN}_LE"
else
    CMDFMT=""
fi

# Run command
CMD0=$HOST_EXE
CMD1="-d -x $FS1 -y $FS2 -i $FN_IN -o $FN_OUT -t $TPLG -a src=libsof_${COMP}.so $CMDFMT"
CMD="$CMD0 $CMD1"
export LD_LIBRARY_PATH=$HOST_LIB

# Run test bench
echo "Command: $CMD0"
echo "Arg:     $CMD1"
$CMD

#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022-2026 Intel Corporation. All rights reserved.

set -e

RAW_INPUT_S16=in_s16.raw
RAW_INPUT_S24=in_s24.raw
RAW_INPUT_S32=in_s32.raw
RAW_OUTPUT_S16=mfcc_s16.raw
RAW_OUTPUT_S24=mfcc_s24.raw
RAW_OUTPUT_S32=mfcc_s32.raw

VALGRIND="valgrind --leak-check=full"
TESTBENCH=$SOF_WORKSPACE/sof/tools/testbench/build_testbench/install/bin/sof-testbench4
TOPOLOGY_S16=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfcc16.tplg
TOPOLOGY_S24=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfcc24.tplg
TOPOLOGY_S32=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfcc32.tplg
OPT_S16="-r 16000 -c 2 -b S16_LE -p 3,4 -t $TOPOLOGY_S16"
OPT_S24="-r 16000 -c 2 -b S24_LE -p 3,4 -t $TOPOLOGY_S24"
OPT_S32="-r 16000 -c 2 -b S32_LE -p 3,4 -t $TOPOLOGY_S32"

# Convert input audio file raw 16 kHz 2 channel 16 bit
sox -R --encoding signed-integer "$1" -L -r 16000 -c 2 -b 16 "$RAW_INPUT_S16"
sox -R --no-dither --encoding signed-integer -L -r 16000 -c 2 -b 16 "$RAW_INPUT_S16" -b 32 "$RAW_INPUT_S32"
sox -R --no-dither --encoding signed-integer -L -r 16000 -c 2 -b 16 "$RAW_INPUT_S16" -b 32 "$RAW_INPUT_S24" vol 0.003906250000

# Run testbench
$VALGRIND $TESTBENCH $OPT_S16 -i "$RAW_INPUT_S16" -o "$RAW_OUTPUT_S16"
$VALGRIND $TESTBENCH $OPT_S24 -i "$RAW_INPUT_S24" -o "$RAW_OUTPUT_S24"
$VALGRIND $TESTBENCH $OPT_S32 -i "$RAW_INPUT_S32" -o "$RAW_OUTPUT_S32"

echo ----------------------------------------------------------------------------------
echo The MFCC data was output to file $RAW_OUTPUT_S16, $RAW_OUTPUT_S24, $RAW_OUTPUT_S32
echo ----------------------------------------------------------------------------------

RAW_OUTPUT_S16=mel_s16.raw
RAW_OUTPUT_S24=mel_s24.raw
RAW_OUTPUT_S32=mel_s32.raw

TESTBENCH=$SOF_WORKSPACE/sof/tools/testbench/build_testbench/install/bin/sof-testbench4
TOPOLOGY_S16=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel16.tplg
TOPOLOGY_S24=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel24.tplg
TOPOLOGY_S32=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel32.tplg
OPT_S16="-r 16000 -c 2 -b S16_LE -p 3,4 -t $TOPOLOGY_S16"
OPT_S24="-r 16000 -c 2 -b S24_LE -p 3,4 -t $TOPOLOGY_S24"
OPT_S32="-r 16000 -c 2 -b S32_LE -p 3,4 -t $TOPOLOGY_S32"

# Run testbench
$VALGRIND $TESTBENCH $OPT_S16 -i "$RAW_INPUT_S16" -o "$RAW_OUTPUT_S16"
$VALGRIND $TESTBENCH $OPT_S24 -i "$RAW_INPUT_S24" -o "$RAW_OUTPUT_S24"
$VALGRIND $TESTBENCH $OPT_S32 -i "$RAW_INPUT_S32" -o "$RAW_OUTPUT_S32"

echo ----------------------------------------------------------------------------------
echo The MFCC Mel data was output to file $RAW_OUTPUT_S16, $RAW_OUTPUT_S24, $RAW_OUTPUT_S32
echo ----------------------------------------------------------------------------------

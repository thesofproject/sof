#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022-2026 Intel Corporation. All rights reserved.

set -e

RAW_INPUT=in.raw
RAW_OUTPUT=mfcc.raw

TESTBENCH=$SOF_WORKSPACE/sof/tools/testbench/build_testbench/install/bin/sof-testbench4
TOPOLOGY=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfcc16.tplg
OPT="-r 16000 -c 2 -b S16_LE -p 3,4 -t $TOPOLOGY -i $RAW_INPUT -o $RAW_OUTPUT"

# Convert input audio file raw 16 kHz 1 channel 16 bit
sox --encoding signed-integer "$1" -L -r 16000 -c 1 -b 16 "$RAW_INPUT"

# Run testbench
$TESTBENCH $OPT -i "$RAW_INPUT" -o "$RAW_OUTPUT"

echo -----------------------------------------------
echo   The MFCC data was output to file $RAW_OUTPUT
echo -----------------------------------------------

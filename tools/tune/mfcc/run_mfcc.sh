#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation. All rights reserved.

set -e

RAW_INPUT=in.raw
RAW_OUTPUT=mfcc.raw

export LD_LIBRARY_PATH=../../testbench/build_testbench/sof_ep/install/lib:../../testbench/build_testbench/sof_parser/install/lib

TESTBENCH=../../testbench/build_testbench/install/bin/testbench
OPT="-q -r 16000 -R 16000 -c 1 -n 1 -b S16_LE -t ../../build_tools/test/topology/test-playback-ssp5-mclk-0-I2S-mfcc-s16le-s16le-48k-24576k-codec.tplg"

# Convert input audio file raw 16 kHz 1 channel 16 bit
sox --encoding signed-integer "$1" -L -r 16000 -c 1 -b 16 "$RAW_INPUT"

# Run testbench
$TESTBENCH $OPT -i "$RAW_INPUT" -o "$RAW_OUTPUT"

echo -----------------------------------------------
echo   The MFCC data was output to file $RAW_OUTPUT
echo -----------------------------------------------

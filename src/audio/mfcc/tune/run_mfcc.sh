#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022-2026 Intel Corporation. All rights reserved.

set -e

RAW_INPUT_S16=in_s16.raw
RAW_INPUT_S24=in_s24.raw
RAW_INPUT_S32=in_s32.raw

VALGRIND="valgrind --leak-check=full"
#VALGRIND=""
TESTBENCH=$SOF_WORKSPACE/sof/tools/testbench/build_testbench/install/bin/sof-testbench4
TESTBENCH_RUN="$VALGRIND $TESTBENCH"

convert_input() {
	sox -R --encoding signed-integer "$1" -L -r 16000 -c 2 -b 16 "$RAW_INPUT_S16"
	sox -R --no-dither --encoding signed-integer -L -r 16000 -c 2 -b 16 \
		"$RAW_INPUT_S16" -b 32 "$RAW_INPUT_S32"
	sox -R --no-dither --encoding signed-integer -L -r 16000 -c 2 -b 16 \
		"$RAW_INPUT_S16" -b 32 "$RAW_INPUT_S24" vol 0.003906250000
}

run_testbench() {
	local tplg_base="$1"
	local out_s16="$2"
	local out_s24="$3"
	local out_s32="$4"
	local label="$5"
	local tplg_s16="${SOF_WORKSPACE}/sof/tools/build_tools/topology/topology2/development/${tplg_base}16.tplg"
	local tplg_s24="${SOF_WORKSPACE}/sof/tools/build_tools/topology/topology2/development/${tplg_base}24.tplg"
	local tplg_s32="${SOF_WORKSPACE}/sof/tools/build_tools/topology/topology2/development/${tplg_base}32.tplg"

	$TESTBENCH_RUN -r 16000 -c 2 -b S16_LE -p 3,4 -t "$tplg_s16" -i "$RAW_INPUT_S16" -o "$out_s16"
	$TESTBENCH_RUN -r 16000 -c 2 -b S24_LE -p 3,4 -t "$tplg_s24" -i "$RAW_INPUT_S24" -o "$out_s24"
	$TESTBENCH_RUN -r 16000 -c 2 -b S32_LE -p 3,4 -t "$tplg_s32" -i "$RAW_INPUT_S32" -o "$out_s32"

	echo ----------------------------------------------------------------------------------
	echo "The ${label} data was output to file ${out_s16}, ${out_s24}, ${out_s32}"
	echo ----------------------------------------------------------------------------------
}

main() {
	convert_input "$1"
	run_testbench "sof-hda-benchmark-mfcc" mfcc_s16.raw mfcc_s24.raw mfcc_s32.raw "MFCC"
	run_testbench "sof-hda-benchmark-mfccmel" mel_s16.raw mel_s24.raw mel_s32.raw "MFCC Mel"

	if [ -n "$XTENSA_PATH" ]; then
		TESTBENCH_RUN="$XTENSA_PATH/xt-run $SOF_WORKSPACE/sof/tools/testbench/build_xt_testbench/sof-testbench4"
		run_testbench "sof-hda-benchmark-mfcc" xt_mfcc_s16.raw xt_mfcc_s24.raw xt_mfcc_s32.raw "Xtensa MFCC"
		run_testbench "sof-hda-benchmark-mfccmel" xt_mel_s16.raw xt_mel_s24.raw xt_mel_s32.raw "Xtensa MFCC Mel"
	fi
}

main "$@"

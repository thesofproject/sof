#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2020 Intel Corporation. All rights reserved.

# stop on most errors
set -e

usage ()
{
    cat <<EOFHELP
Usage:     $0 <options> <comp direction bits_in bits_out fs_in fs_out input output>
Example 1: $0 volume playback 16 16 48000 48000 input.raw output.raw
Example 2: $0 -e volume_trace.txt -t volume_config.sh

Where volume_config.sh could be e.g. next. Minimal configuration need is only
the COMP line.

# Volume component configuration
COMP=volume
DIRECTION=playback
BITS_IN=16
BITS_OUT=16
CHANNELS_IN=2
CHANNELS_OUT=2
FS_IN=48000
FS_OUT=48000
FN_IN=input.raw
FN_OUT=output.raw
FN_TRACE:=trace.txt # This is default value if FN_TRACE is not set via -e option
VALGRIND=yes
EOFHELP
}

parse_args ()
{
    # Defaults
    DIRECTION=playback
    BITS_IN=16
    BITS_OUT=
    CHANNELS_IN=2
    CHANNELS_OUT=
    FS_IN=48000
    FS_OUT=
    VALGRIND=true
    DEBUG=
    SOURCED_CONFIG=false
    FN_TRACE=
    EXTRA_OPTS=

    while getopts ":he:t:" opt; do
	case "${opt}" in
	    e)
		FN_TRACE="${OPTARG}"
		;;
	    h)
		usage
		exit
		;;
	    t)
		# shellcheck disable=SC1090
		source "${OPTARG}"
		SOURCED_CONFIG=true
		;;
	    *)
		usage
		exit 1
		;;
	esac
    done

    shift $((OPTIND -1))

    if ! "$SOURCED_CONFIG"; then
	[ $# -eq 8 ] || {
	    usage "$0"
	    exit 1
	}

	COMP="$1"
	DIRECTION="$2"
	BITS_IN="$3"
	BITS_OUT="$4"
	FS_IN="$5"
	FS_OUT="$6"
	FN_IN="$7"
	FN_OUT="$8"
    fi

    if [[ -z $BITS_OUT ]]; then
	BITS_OUT=$BITS_IN
    fi

    if [[ -z $FS_OUT ]]; then
	FS_OUT=$FS_IN
    fi

    if [[ -z $CHANNELS_OUT ]]; then
	CHANNELS_OUT=$CHANNELS_IN
    fi
}

delete_file_check ()
{
    if [ -f "$1" ]; then
	rm -f "$1"
    fi
}

run_testbench ()
{
    delete_file_check "$FN_OUT"
    delete_file_check "$FN_TRACE"
    if [ -z "$FN_TRACE" ]; then
	# shellcheck disable=SC2086
	$VALGRIND_CMD $CMD
    else
	# shellcheck disable=SC2086
	$VALGRIND_CMD $CMD 2> "$FN_TRACE"
    fi
}

parse_args "$@"

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
OPTS="$DEBUG -r $FS_IN -R $FS_OUT -c $CHANNELS_IN -n $CHANNELS_OUT $BINFMT -t $TPLG"
DATA="-i $FN_IN -o $FN_OUT"
ARG="$OPTS $EXTRA_OPTS $DATA"
CMD="$HOST_EXE $ARG"
if "$VALGRIND"; then
    VALGRIND_CMD="valgrind --leak-check=yes --error-exitcode=1"
else
    VALGRIND_CMD=
fi

export LD_LIBRARY_PATH=$HOST_LIB:$TPLG_LIB

# Run test bench
echo "Command:         $HOST_EXE"
echo "Argument:        $ARG"
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"

run_testbench

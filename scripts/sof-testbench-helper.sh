#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause

set -e

usage() {
    echo "Usage: $0 <options>"
    echo "Options"
    echo "  -b <bits>, default 32"
    echo "  -c <channels>, default 2"
    echo "  -h shows this text"
    echo "  -i <input wav>, default /usr/share/sounds/alsa/Front_Center.wav"
    echo "  -k keep temporary files in /tmp"
    echo "  -m <module>, default gain"
    echo "  -n <pipelines>, default 1,2"
    echo "  -o <output wav>, default none"
    echo "  -p <profiling result text>, use with -x, default none"
    echo "  -r <rate>, default 48000"
    echo "  -t <force topology>, default none, e.g. production/sof-hda-generic.tplg"
    echo "  -v runs with valgrind, not available with -x"
    echo "  -x runs testbench with xt-run simulator"
    echo
    echo "Example: run DRC with xt-run with profiling (slow)"
    echo "$0 -x -m drc -p profile-drc32.txt"
    echo
    echo "Example: process with native build DRC file Front_Center.wav (fast)"
    echo "$0 -m drc -i /usr/share/sounds/alsa/Front_Center.wav -o ~/tmp/Front_Center_with_DRC.wav"
    echo
    echo "Example: check component eqiir with valgrind"
    echo "$0 -v -m eqiir"
    echo
    echo This script must be run from the sof firmware toplevel or workspace directory
}

# First check for the workspace environment variable
if [ -z "$SOF_WORKSPACE" ]; then
    # Environment variable is empty or unset so use default
    BASE_DIR="$HOME/work/sof"
else
    # Environment variable exists and has a value
    BASE_DIR="$SOF_WORKSPACE"
fi
cd "$BASE_DIR"

# check we are in the workspace directory
if [ ! -d "sof" ]; then
    echo "Error: can't find SOF firmware directory. Please check your installation."
    exit 1
fi

OUTWAV=
CLIP=/usr/share/sounds/alsa/Front_Center.wav
MODULE=gain
BITS=32
RATE_IN=48000
RATE_OUT=48000
CHANNELS_IN=2
CHANNELS_OUT=2
PIPELINES="1,2"
INFILE1=$(mktemp --tmpdir=/tmp in-XXXX.raw)
OUTFILE1=$(mktemp --tmpdir=/tmp out-XXXX.raw)
TRACEFILE=$(mktemp --tmpdir=/tmp trace-XXXX.txt)
PROFILEOUT=$(mktemp --tmpdir=/tmp profile-XXXX.out)
KEEP_TMP=false
XTRUN=false
PROFILE=false
TPLG0=
VALGRIND=

while getopts "b:c:hi:km:n:o:p:r:t:vx" opt; do
    case "${opt}" in
        b)
	    BITS=${OPTARG}
	    ;;
        c)
	    CHANNELS_IN=${OPTARG}
	    CHANNELS_OUT=${OPTARG}
	    ;;
        h)
	    usage
	    exit
	    ;;
        i)
	    CLIP=${OPTARG}
	    ;;
        k)
	    KEEP_TMP=true
	    ;;
        m)
	    MODULE=${OPTARG}
	    ;;
        n)
	    PIPELINES=${OPTARG}
	    ;;
        o)
	    OUTWAV=${OPTARG}
	    ;;
        p)
	    PROFILETXT=${OPTARG}
	    PROFILE=true
	    ;;
        r)
	    RATE_IN=${OPTARG}
	    RATE_OUT=${OPTARG}
	    ;;
        t)
	    TPLG0=${OPTARG}
	    ;;
        v)
	    VALGRIND=valgrind
	    ;;
        x)
	    XTRUN=true
	    ;;
        *)
	    usage
	    exit
	    ;;
    esac
done
shift $((OPTIND-1))

echo Converting clip "$CLIP" to raw input
if [[ "$BITS" == "24" ]]; then
    # Sox does not support S24_4LE format
    # Note gain 0.00390615 is empically find just a bit lower gain than
    # 1/256 = 0.00390625 that doesn't cause sox rounding to exceed
    # INT24_MIN .. INT24_MAX range.
    sox --encoding signed-integer "$CLIP" -L -r "$RATE_IN" -c "$CHANNELS_IN" -b 32 "$INFILE1" vol 0.00390615
else
    sox --encoding signed-integer "$CLIP" -L -r "$RATE_IN" -c "$CHANNELS_IN" -b "$BITS" "$INFILE1"
fi
TOOLSDIR="$PWD/build_tools/testbench"
TB4="$PWD/build-testbench/install/bin/sof-testbench4"
XTB4="$PWD/build-xt-testbench/sof-testbench4"
XTB4_SETUP="$PWD/build-xt-testbench/xtrun_env.sh"
if [ -z "$TPLG0" ]; then
    TPLG="$PWD/build_tools/topology/topology2/development/sof-hda-benchmark-${MODULE}${BITS}.tplg"
else
    TPLG="$PWD/build_tools/topology/topology2/$TPLG0"
fi
FMT="S${BITS}_LE"
OPTS="-r $RATE_IN -R $RATE_OUT -c $CHANNELS_IN -c $CHANNELS_OUT -b $FMT -p $PIPELINES -t $TPLG -i $INFILE1 -o $OUTFILE1"

if [ ! -f "$TPLG" ]; then
    echo "Error: Topology $TPLG is not found"
    echo "Build with scripts/build-tools.sh -Y"
    exit 1
fi

if [[ "$XTRUN" == true ]]; then
    if [ ! -x "$XTB4" ]; then
	echo "Error: No executable found from $XTB4"
	echo "Build with scripts/rebuild-testbench.sh -p <platform>"
	exit 1
    fi
    echo "Running xtensa testbench"
    echo "  topology: $TPLG"
    echo "  input: $INFILE1, output: $OUTFILE1, trace: $TRACEFILE, profile: $PROFILETXT"
    source "$XTB4_SETUP"
    if [[ $PROFILE == true ]]; then
	"$XTENSA_PATH"/xt-run --profile="$PROFILEOUT" "$XTB4" $OPTS 2> "$TRACEFILE"
	"$XTENSA_PATH"/xt-gprof "$XTB4" "$PROFILEOUT" > "$PROFILETXT"
    else
	"$XTENSA_PATH"/xt-run "$XTB4" $OPTS 2> "$TRACEFILE"
    fi
else
    if [ ! -x "$TB4" ]; then
	echo "Error: No executable found from $TB4"
	exit 1
    fi
    echo "Running testbench"
    echo "  topology: $TPLG"
    echo "  input: $INFILE1, output: $OUTFILE1, trace: $TRACEFILE"
    $VALGRIND "$TB4" $OPTS 2> "$TRACEFILE" || {
        cat "$TRACEFILE"
        exit $?
    }
    if [ -n "$VALGRIND" ]; then
	cat "$TRACEFILE"
    fi
fi

if [ -n "$OUTWAV" ]; then
    echo Converting raw output to "$OUTWAV"
    if [[ "$BITS" == "24" ]]; then
	sox --encoding signed-integer -L -r "$RATE_OUT" -c "$CHANNELS_OUT" -b 32 "$OUTFILE1" "$OUTWAV" vol 256
    else
	sox --encoding signed-integer -L -r "$RATE_OUT" -c "$CHANNELS_OUT" -b "$BITS" "$OUTFILE1" "$OUTWAV"
    fi
fi

if [[ "$KEEP_TMP" == false ]]; then
    echo Deleting temporary files
    rm -f "$INFILE1" "$OUTFILE1" "$TRACEFILE" "$PROFILEOUT"
fi

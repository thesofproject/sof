#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause

set -e

usage() {
    echo "Usage: $0 <options>"
    echo "  -d <directory> directory to place code profiling reports"
    echo "  -h shows this text"
    echo "  -p <platform> sets platform for scripts/rebuild-testbench.sh"
    echo
    echo "This script must be run from the sof firmware toplevel or workspace directory"
}

MODULES_S32="asrc dcblock drc drc_multiband eqfir eqiir gain src tdfb"
MODULES_S24="aria"

# This script can be executed from the sof firmware toplevel or workspace directory
# First check for the worksapce environment variable
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

#set default values
PLATFORM=none
PDIR=$PWD/testbench/profile

while getopts "hp:d:" opt; do
    case "${opt}" in
        d)
	    PDIR=${OPTARG}
	    ;;
        h)
	    usage
	    exit
	    ;;
        p)
	    PLATFORM=${OPTARG}
	    ;;
        *)
	    usage
	    exit
	    ;;
    esac
done
shift $((OPTIND-1))

# Build
SCRIPTS=$PWD/sof/scripts
mkdir -p "$PDIR"
"$SCRIPTS"/rebuild-testbench.sh -p "$PLATFORM"
HELPER="$SCRIPTS"/sof-testbench-helper.sh

echo "Profiler reports are stored to $PDIR"

# Run sof-hda-generic.tplg playback
echo "Profiling sof-hda-generic.tplg ..."
$HELPER -x -t production/sof-hda-generic.tplg -n 1,2 \
	-p "$PDIR/profile-$PLATFORM-generic.txt" > "$PDIR/log-$PLATFORM-generic.txt"

# Run sof-hda-benchmark-generic.tplg playback
echo "Profiling sof-hda-benchmark-generic.tplg  ..."
$HELPER -x -t development/sof-hda-benchmark-generic.tplg -n 1,2,3 \
	-p "$PDIR/profile-$PLATFORM-benchmark.txt" > "$PDIR/log-$PLATFORM-benchmark.txt"

# Profile modules
for mod in $MODULES_S32
do
    echo "Profiling $mod ..."
    $HELPER -x -m "$mod" -p "$PDIR/profile-$PLATFORM-$mod.txt" > "$PDIR/log-$PLATFORM-$mod.txt"
done

for mod in $MODULES_S24
do
    echo "Profiling $mod ..."
    $HELPER -b 24 -x -m "$mod" -p "$PDIR/profile-$PLATFORM-$mod.txt" > "$PDIR/log-$PLATFORM-$mod.txt"
done

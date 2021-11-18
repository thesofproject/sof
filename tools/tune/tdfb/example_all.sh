#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation. All rights reserved.

set -e

CONFIG_LIST=( example_pass_config example_line_array
	      example_line_0mm36mm146mm182mm example_circular_array example_two_beams )
OCTAVE_CMD=( octave --no-gui --quiet )
MATLAB_CMD=( matlab -batch )

main () {
    if command -v matlab &> /dev/null; then
	echo "Using Matlab"
	CMD=( "${MATLAB_CMD[@]}" )
	EXT=
    elif command -v octave &> /dev/null; then
	echo "Using Octave"
	CMD=( "${OCTAVE_CMD[@]}" )
	EXT=".m"
    else
	echo "Please install Matlab or Octave to run this script."
	exit 1
    fi
    export_configurations
}

export_configurations()
{
    for cfg in "${CONFIG_LIST[@]}" ; do
	"${CMD[@]}" "${cfg}""${EXT}"
    done
}

main "$@"

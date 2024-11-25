#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022-2024 Intel Corporation.

set -e

CONFIG_LIST=( sof_example_pass_config sof_example_line_array
	      sof_example_line_0mm36mm146mm182mm sof_example_circular_array
	      sof_example_two_beams sof_example_two_beams_default )
OCTAVE_CMD=( octave --no-window-system )
MATLAB_CMD=( matlab -nodisplay -batch )

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

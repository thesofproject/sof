#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2020 Intel Corporation. All rights reserved.

# stop on most errors
set -e

usage ()
{
    echo "Usage:   $1 <bits in> <bits out> <rate in> <rate out> <input> <output>"
    echo "Example: $1 16 16 32000 48000 input.raw output.raw"
}

main()
{
    local COMP DIRECTION

    if [ $# -ne 6 ]; then
	usage "$0"
	exit 1
    fi

    COMP=src
    DIRECTION=playback

    ./comp_run.sh $COMP $DIRECTION "$@"
}

main "$@"

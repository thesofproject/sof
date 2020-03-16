#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation. All rights reserved.

if [ -z "$5" ]; then
    echo "Usage:   $0 <bits in> <bits out> <rate> <input> <output>"
    echo "Example: $0 16 16 48000 input.raw output.raw"
    exit
fi


COMP=eq-iir
DIRECTION=playback

./comp_run.sh $COMP $DIRECTION $1 $2 $3 $3 $4 $5

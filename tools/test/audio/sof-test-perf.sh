#!/bin/sh

# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation. All rights reserved.
# Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

octave --no-gui sof_test_perf_top.m
if [ $? -eq 0 ]; then
    echo "Test passed."
    exit 0
else
    echo "Test failed." >&2
    exit 1
fi

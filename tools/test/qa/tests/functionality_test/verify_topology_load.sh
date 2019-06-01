#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Verify DSP topology loading
dmesg | grep "loading topology" > /dev/null
if [ $? != 0 ]; then
        echo "Fail: DSP topology was not loaded."
        exit 1
else
        exit 0
fi


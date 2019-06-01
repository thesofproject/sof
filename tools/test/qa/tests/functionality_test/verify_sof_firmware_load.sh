#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Verify DSP firmware is loaded
#dmesg | grep "FW loaded" > /dev/null
dmesg | grep "firmware boot complete" > /dev/null
if [ $? != 0 ]; then
        echo "Fail: DSP firmware were not loaded."
        exit 1
else
        exit 0
fi


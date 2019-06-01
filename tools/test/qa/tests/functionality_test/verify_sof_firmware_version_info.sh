#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Verify DSP topology loading
dmesg | grep "Firmware info" > /dev/null
if [ $? != 0 ]; then
        echo "Fail: No firmware info exist."
        exit 1
else
        exit 0
fi


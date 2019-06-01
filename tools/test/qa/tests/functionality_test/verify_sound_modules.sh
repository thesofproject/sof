#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Verify DSP sound modules are loaded
lsmod | grep -i snd_soc > /dev/null
if [ $? != 0 ]; then
	echo "Fail: Intel HDA modules were not loaded."
	exit 1
else 
        exit 0
fi

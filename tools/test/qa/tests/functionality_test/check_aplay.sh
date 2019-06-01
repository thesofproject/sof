#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Check aplay function
aplay -D hw:0,0 -f dat -d 5 -c 2 -r 48000 /dev/zero
if [ $? != 0 ]; then
        echo "Aplay FAIL"
	exit 1
else
        echo "APlay PASS"
	exit 0
fi


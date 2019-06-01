#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

#Get mixer properties
amixer -D hw:sofbytcrrt5651 > /dev/null
if [ $? -ne 0 ]; then
	echo "get mixer properties failed"
	exit 1
else
	exit 0
fi

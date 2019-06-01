#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# check dma trace output
sudo rmbox -t > rmbox.log & sleep 2 ; pkill -9 rmbox
# get size of trace log
size=`du -k rmbox.log |awk '{print $1}'`

if [ $size -gt 1 ]; then
	exit 0
else
	exit 1
fi
rm -f rmbox.log

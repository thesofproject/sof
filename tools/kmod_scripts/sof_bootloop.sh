#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

set -e

MAXLOOPS=100
COUNTER=0

while [ $COUNTER -lt $MAXLOOPS ]; do
    echo "test $COUNTER"
    ./sof_bootone.sh
    dmesg > boot_$COUNTER.bootlog
    let COUNTER+=1
    sleep 6
done

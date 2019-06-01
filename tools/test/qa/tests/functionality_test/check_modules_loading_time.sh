#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# reloading the audio modules
criterion_time=50

(time modprobe sof_acpi_dev) >& rl_time.log
reload_time=`cat rl_time.log |grep "real" | awk -F '.' '{print $2}' |cut -c1-3`
echo $reload_time
if [ "$reload_time" -gt "$criterion_time" ]; then
	exit 1
else
	exit 0
fi

sleep 2



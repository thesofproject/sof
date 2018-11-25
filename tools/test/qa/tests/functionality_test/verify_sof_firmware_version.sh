#!/bin/bash

# Verify DSP firmware is loaded
firmware_version=0.9
get_version=`dmesg | grep "FW loaded" |head -1 |grep "version:" |cut -d : -f 4 |cut -c 2-4`
#get_version=`cat /var/log/kern.log | grep "FW loaded" |head -1 |grep "version:" |cut -d : -f 7 |cut -b 2-4`
if [ "$get_version" != "$firmware_version" ]; then
        echo "Fail: DSP firmware were not loaded."
        exit 1
else
        exit 0
fi


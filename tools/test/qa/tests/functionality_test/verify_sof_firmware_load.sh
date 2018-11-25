#!/bin/bash

# Verify DSP firmware is loaded
#dmesg | grep "FW loaded" > /dev/null
dmesg | grep "firmware boot complete" > /dev/null
if [ $? != 0 ]; then
        echo "Fail: DSP firmware were not loaded."
        exit 1
else
        exit 0
fi


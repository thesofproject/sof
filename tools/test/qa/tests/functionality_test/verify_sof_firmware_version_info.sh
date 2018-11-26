#!/bin/bash

# Verify DSP topology loading
dmesg | grep "Firmware info" > /dev/null
if [ $? != 0 ]; then
        echo "Fail: No firmware info exist."
        exit 1
else
        exit 0
fi


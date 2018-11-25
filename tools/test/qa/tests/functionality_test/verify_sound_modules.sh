#!/bin/bash

# Verify DSP sound modules are loaded
lsmod | grep -i snd_soc > /dev/null
if [ $? != 0 ]; then
	echo "Fail: Intel HDA modules were not loaded."
	exit 1
else 
        exit 0
fi

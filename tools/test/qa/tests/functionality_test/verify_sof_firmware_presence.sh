#!/bin/bash

MOD_VER=`lscpu |grep "Model name" |awk -F " " '{print $6}'`
if [ $MOD_VER == "E3826" ] || [ $MOD_VER == "E3845" ]; then
	PLATFORM="byt"
elif [ $MOD_VER == "A3960" ] || [ $MOD_VER == "N4200" ]; then
	PLATFORM="apl"
elif [ $MOD_VER == "0000" ]; then
	PLATFORM="cnl"
fi

# Verify DSP firmware is presence
ls /lib/firmware/intel/sof-$PLATFORM.ri > /dev/null
if [ $? != 0 ]; then
	echo "Fail: DSP firmware doesnot presence."
	exit 1
else 
        exit 0
fi

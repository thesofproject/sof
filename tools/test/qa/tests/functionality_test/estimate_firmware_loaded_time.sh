#!/bin/bash

# Estimate firmware loaded time
t2=`dmesg | grep "firmware boot complete" | awk -F"[" '{print $2}' | awk -F"]" '{print $1}'`
t1=`dmesg | grep "loading firmware" | awk -F"[" '{print $2}' | awk -F"]" '{print $1}'`
t=`echo "$t2 $t1" | awk '{print $1-$2}'` 
# Firmware load time need less than 50ms
if [ $(echo "$t > 0.05" | bc) -eq 1 ]; then
        echo "Fail: Firmware load timeout."
        exit 1
else
        exit 0
fi


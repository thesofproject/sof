#!/bin/bash

#Get mixer properties
amixer -D hw:sofbytcrrt5651 sset 'ADC' 127 > /dev/null
if [ $? -ne 0 ]; then
	echo "set ADC volume to maximum FAIL"
	exit 1
else
	exit 0
fi

#!/bin/bash

# Check line in/out function
rm -rf playback.result
RATE=$1
if [ $RATE == "48K" ]; then
	RATE=48000
fi

FORMAT=$2
if [ $FORMAT == "s16le" ]; then
	FORMAT=dat
elif [ $FORMAT == "s24le" ]; then
	FORMAT=S24_LE
elif [ $FORMAT == "s32le" ]; then
	FORMAT=S32_LE
fi

PIPELINE_TYPE=$3
PLATFORM=$4

# amixer setting for line in/out mode 
alsactl restore -f asound_state/$PLATFORM/asound.state.line
sleep 2
alsabat -D hw:0,0 -r $RATE -c 2 -f $FORMAT -F 1500
	if [ $? != 0 ]; then
		echo "Fail: line in/out mode playback test failed."
		echo "Check_"$PIPELINE_TYPE"_line_in_out_"$RATE"_"$FORMAT"_playback FAIL" >> playback.result
		exit 1
	else
		echo "Check_"$PIPELINE_TYPE"_line_in_out_"$RATE"_"$FORMAT"_playback PASS" >> playback.result
		exit 0
	fi

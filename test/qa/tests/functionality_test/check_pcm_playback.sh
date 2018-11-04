#!/bin/bash

# Check playback function
FREQ=("17" "31" "67" "131" "257" "521" "997" "1033" "2069" "4139" "8273" "16547")

rm -rf playback.result
RATE=$1
if [ $RATE == "48k" ]; then
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

# Check passthrough Playback Pipeline
for freq in "${FREQ[@]}"
do
	alsabat -D hw:0,0 -r $RATE -c 2 -f $FORMAT -F $freq
		if [ $? != 0 ]; then
			echo "Fail: playback failed with "$PIPELINE_TYPE" pipeline."
			echo "Check_"$PIPELINE_TYPE"_Playback_"$RATE"_format_"$FORMAT"_freq_"$freq" FAIL" >> playback.result
		else
			echo "Check_"$PIPELINE_TYPE"_Playback_"$RATE"_format_"$FORMAT"_freq_"$freq" PASS" >> playback.result
			echo "Check_"$PIPELINE_TYPE"_Capture_"$RATE"_format_"$FORMAT"_freq_"$freq" PASS" >> playback.result
		fi
done


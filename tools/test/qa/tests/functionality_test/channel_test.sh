#!bin/bash

# channel test for low latency 
rm -rf playback.result

input_freq=12000
bit=S32_LE
# Check Media Playback Pipeline
alsabat -D hw:0,0 -c 2 -F $input_freq:$input_freq -f $bit
if [ $? != 0 ]; then
	echo "Fail: channel test failed with low latency pipeline ."
	echo "channel_test_with_low_latency pipeline_"$input_freq:$input_freq"_Bit_"$bit" FAIL" >> playback.result
else
	echo "channel_test_with_low_latency_pipeline_"$input_freq:$input_freq"_Bit_"$bit" PASS" >> playback.result
fi

# media pipeline check
frequency_L=900
frequency_R=12000
bit=S32_LE
# Check Media Playback Pipeline
alsabat -P hw:0,1 -C hw:0,0 -c 2 -F $frequency_L:$frequency_R -f $bit
if [ $? != 0 ]; then
	echo "Fail: channel test failed with media pipeline ."
	echo "channel_test_with_media_pipeline_"$frequency_L:$frequency_R"_Bit_"$bit" FAIL" >> playback.result
else
	echo "channel_test_with_media_pipeline_"$frequency_L:$frequency_R"_Bit_"$bit" PASS" >> playback.result
fi


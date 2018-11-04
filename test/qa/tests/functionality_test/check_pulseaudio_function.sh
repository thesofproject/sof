#!/bin/bash

# Check playback function
rm -rf playback.result
#inputsample=(8000 11025 12000 16000 18900 22050 24000 32000 44100 48000 64000 88200 96000 176400 192000)
inputsample=(8000, 16000, 24000, 32000, 44100, 48000)
bitlist=(U8 S16_LE S24_3LE S32_LE)
# Check Media Playback Pipeline
for samplerate in ${inputsample[*]};do
	for bit in ${bitlist[*]}; do
		alsabat -P hw:0,1 -C hw:0,0 -r $samplerate -c 2 -f $bit
		if [ $? != 0 ]; then
			echo "Fail: playback failed with Media pipeline."
			echo "Check_Media_Playback_Pipeline_Playback_"$samplerate"_Bit_"$bit" FAIL" >> playback.result
		else
			echo "Check_Media_Playback_Pipeline_Playback_"$samplerate"_Bit_"$bit" PASS" >> playback.result
		fi
	done
done
# Check Low Latency Playback Pipeline
for bit in S16_LE S24_3LE S32_LE ; do
	alsabat -D hw:0,0 -r 48000 -c 2 -f $bit
	if [ $? != 0 ]; then
		echo "Fail: playback failed with low latency pipeline."
		echo "Check_Low_Latency_Pipeline_Playback_48000_Bit_"$bit" FAIL" >> playback.result
	else
		echo "Check_Low_latency_Pipeline_Playback_48000_Bit_"$bit" PASS" >> playback.result
	fi
done

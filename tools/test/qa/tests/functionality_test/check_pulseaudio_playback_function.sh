#!/bin/bash

# Check playback function
/usr/bin/pulseaudio --system &
sleep 10

alsactl restore -f asound_state/asound.state.pulse-p
sleep 2
rm -rf playback.result
#inputsample=(8000 11025 12000 16000 18900 22050 24000 32000 44100 48000 64000 88200 96000 176400 192000)
alsabat -Ppulse -r 48000 -c 2 -F 1500 -Chw:0,0
if [ $? != 0 ]; then
	echo "Fail: pulseaudio playback failed with passthrough pipeline."
	echo "Check_pulseaudio_Playback FAIL" >> playback.result
else
	echo "Check_pulseaudio_Playback PASS" >> playback.result
fi


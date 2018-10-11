#!/bin/bash

# Check capture function
ps -aux |grep pulseaudio
if [ $? != 0 ]; then
	usr/bin/pulseaudio --system &
	sleep 10
else
    echo "Pass: start pulseaudio server passed"
fi
alsactl restore -f asound_state/asound.state.pulse-c
sleep 2
rm -rf playback.result
#inputsample=(8000 11025 12000 16000 18900 22050 24000 32000 44100 48000 64000 88200 96000 176400 192000)
alsabat -Phw:0,0 -r 48000 -c 2 -F 1500 -Cpulse
if [ $? != 0 ]; then
	echo "Fail: pulseaudio capture failed with passthrough pipeline."
	echo "Check_pulseaudio_capture FAIL" >> playback.result
else
	echo "Check_pulseaudio_capture PASS" >> playback.result
fi


#!/bin/bash

#check playback after modules reloaded
pkill -9 alsabat
sleep 2

alsactl restore -f asound_state/asound.state-1.0-passthrough-16bit
sleep 2
alsabat -D hw:0,0 -r 48000 -c 2 -f dat -F 1500
if [ $? -eq 0 ]; then
	exit 0
else
	exit 1
fi


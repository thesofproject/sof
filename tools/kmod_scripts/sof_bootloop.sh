#!/bin/bash

enable_pulseaudio="; autospawn = yes"
disable_pulseaudio="  autospawn = no"

sed -i "s/$enable_pulseaudio/$disable_pulseaudio/" /etc/pulse/client.conf
killall pulseaudio

set -e

MAXLOOPS=100
COUNTER=0

while [ $COUNTER -lt $MAXLOOPS ]; do
    echo "test $COUNTER"
    ./sof_bootone.sh
    dmesg > boot_$COUNTER.bootlog
    let COUNTER+=1
done

sed -i "s/$disable_pulseaudio/$enable_pulseaudio/" /etc/pulse/client.conf

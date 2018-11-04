#!/bin/bash

# Check playback pcm list.
aplay -l |grep "device"> /dev/null
if [ $? != 0 ]; then
        echo "Fail: Can not get playback pcm list."
        exit 1
else
   exit 0
fi


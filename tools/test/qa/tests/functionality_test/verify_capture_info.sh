#!/bin/bash

#diff test_scripts/pcm0cinfo /proc/asound/card0/pcm0c/info > /tmp/capinfdiff.log
cat /proc/asound/sofbytcrrt5651/pcm0c/info |grep "stream: CAPTURE" > /dev/null
if [ $? -ne 0 ]; then
        echo "Fail: DSP capture info doesnot match"
#        echo "The diff is:"
#        cat /tmp/capinfdiff.log | while read line
#        do
#                echo $line
#        done
        exit 1
else
        exit 0
fi


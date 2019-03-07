#!/bin/bash
# suspend & resume the device
# param: $1 loop cnt, $2 max s3 interval, $3 hw_params
function pm_s3_loop
{
    local cnt=$1
    local interval=$2
    local hw_params=$3
    local pbPID=
    local cpPID=
    local type=
    local dev=
    local rate=
    local channel=
    local fmt=
    local audioSrc=
    if [[ -n $hw_params ]]; then
        type=$(dict_value $hw_params 'type')
        dev=$(dict_value $hw_params 'dev')
        rate=$(dict_value $hw_params 'rate')
        channel=$(dict_value $hw_params 'channel')
        fmt=$(dict_value $hw_params 'fmt')

        audioSrc=$AUDIO_SRC/test-c$channel-r$rate-f$fmt-d60.raw
        [[ ! -f $audioSrc ]] && audioSrc=/dev/zero

        if [[ $type == both ]]; then
            aplay -D $dev -r $rate -c $channel -f $fmt $audioSrc & pbPID=$!
            arecord -D $dev -r $rate -c $channel -f $fmt /dev/null & cpPID=$!
        elif [[ $type == playback ]]; then
            aplay -D $dev -r $rate -c $channel -f $fmt $audioSrc & pbPID=$!
        elif [[ $type == capture ]]; then
            arecord -D $dev -r $rate -c $channel -f $fmt /dev/null & cpPID=$!
        fi
    fi

    local ret=0
    local i=0
    while [[ $i -lt $cnt ]]; do
        if [[ $i -ne 0 && $((i % 100)) -eq 0 ]]; then
            echo -e "#"
        else
            echo -en "#"
        fi
        sleep $(($RANDOM % $interval + 1))
        if [[ ! -d /proc/$pbPID && -n $hw_params ]]; then
            if [[ $type == both || $type == playback ]]; then
                aplay -D $dev -r $rate -c $channel -f $fmt $audioSrc & pbPID=$!
            fi
        fi

        let wkTime=$(($RANDOM % $interval + 1))
        sudo rtcwake -m mem -s $wkTime > /dev/null
        ret=$?
        [[ $ret -ne 0 ]] && break
        let i+=1
    done
    [[ -n $pbPID && -d /proc/$pbPID ]] && kill -9 $pbPID
    [[ -n $cpPID && -d /proc/$cpPID ]] && kill -9 $cpPID
    return $ret
}

declare -fx pm_s3_loop

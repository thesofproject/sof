#!/bin/bash

# check the speaker if it is available
# param: $1 device, $2 rate, $3 fmt, $4 channel, $5 test count
function check_speaker
{
    local device=$1
    local rate=$2
    if [[ ${rate} != `echo $rate | sed 's/[^0-9.]//g'` ]]; then
        if [[ ${rate:${#rate}-1:1} =~ [k|K] ]]; then
            rate=$(echo "${rate:0:-1} * 1000" | bc)
            rate=${rate%.*}
        else
            logw "wrong input rate: "$rate
            exit 1
        fi
    fi

    local fmt=$3
    [[ $fmt == ${fmt//_/} ]] && fmt=${fmt:0:3}_${fmt:3:2}

    local channel=$4
    local tcnt=$5
    [[ -z $tcnt ]] && tcnt=0
    speaker-test -D $device -r $rate -c $channel -f $fmt -l $tcnt -t wav -P 8
    return $?
}

declare -fx check_speaker

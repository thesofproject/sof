#!/bin/bash
# Check playback function$

# check the pcm playback
# param: $1 device, $2 rate, $3 fmt, $4 channel
function check_pcm_playback
{
    local testFreq=("17" "31" "67" "131" "257" "521" "997" "1033" "2069" "4139" "8273" "16547")
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
    fmt=${fmt:0:3}_${fmt:3:2}

    local channel=$4
    local passedCnt=0
    for freq in ${testFreq[@]}; do
        alsabat -P $device -r $rate -c $channel -f $fmt -F $freq 1>/dev/null 2>&1
        if [[ $? -eq 0 ]]; then
            logi "check pcm playback ($rate, $fmt, $freq) passed"
            passedCnt=$((passedCnt + 1))
        else
            logi "check pcm playback ($rate, $fmt, $freq) failed"
        fi
    done

    [[ $passedCnt -eq ${#testFreq[@]} ]] && return 0 || return 1
}

declare -fx check_pcm_playback

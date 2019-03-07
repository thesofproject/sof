#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

function __case_passed
{
    __OCCUPY_LINE_DELETE_ME__ #case passed post response
}

function __case_failed
{
    __OCCUPY_LINE_DELETE_ME__ #case failed post response
}

function __execute
{
    retrieve_tplg retrieved_param
    return $?
}

# retrieve topology callback function
# param: $1 alsa params
function retrieved_param
{
    local params=$1
    [[ -z $params ]] && return 1

    local duration=$((10 * 60))
    local type=$(dict_value $params 'type')
    local dev=$(dict_value $params 'dev')
    local rate=$(dict_value $params 'rate')
    local channel=$(dict_value $params 'channel')
    local fmt=$(dict_value $params 'fmt')

    # e.g.: test-c2-r48000-fs32_le-d600.raw
    local audioSrc=$AUDIO_SRC/test-c$channel-r$rate-f$fmt-d$duration.raw
    [[ ! -f $audioSrc ]] && audioSrc=/dev/zero

    if [[ $type == playback || $type == both ]]; then
        aplay -D $dev -r $rate -c $channel -f $fmt -d $duration $audioSrc
    fi
}

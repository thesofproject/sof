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
    local param=$1
    [[ -z $param ]] && return 1

    local dev=$(dict_value $param 'dev')
    local rate=$(dict_value $param 'rate')
    local channel=$(dict_value $param 'channel')
    local fmt=$(dict_value $param 'fmt')

    check_pcm_playback $dev $rate $fmt $channel
}

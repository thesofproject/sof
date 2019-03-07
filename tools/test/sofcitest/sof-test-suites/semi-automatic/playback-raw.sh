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
    wait_confirmed
    return $?
}

# retrieve topology callback function
# param: $1 alsa params
function retrieved_param
{
    local params=$1
    [[ -z $params ]] && return 1

    params=$(dict_update $params "time=0.5")
    params=$(dict_update $params "count=100")

    local duration=60
    local channel=$(dict_value $params 'channel')
    local rate=$(dict_value $params 'rate')
    local fmt=$(dict_value $params 'fmt')

    local type=$(dict_value $params 'type')
    if [[ "$type" == "playback" ]]; then
        local audioSrc=$RESOURCE_ROOT/raw/test-c$channel-r$rate-f$fmt-d$duration.raw
        params=$(dict_update $params "file=$audioSrc")
        pause_resume_playback $params
    else
        local recOutput=$REPORT_ROOT/rec/record-c$channel-r$rate-f$fmt.wav
        pause_resume_capture $params
    fi
}

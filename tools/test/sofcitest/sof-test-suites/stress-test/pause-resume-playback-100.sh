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

    params=$(dict_update $params 'time=0.5')
    params=$(dict_update $params 'count=100')

    local type=$(dict_value $params 'type')

    if [[ "$type" == "playback" ]]; then
        pause_resume_playback $params
    else
        pause_resume_capture $params
    fi
}

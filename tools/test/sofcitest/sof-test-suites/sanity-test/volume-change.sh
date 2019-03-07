#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

##############################################################################
#| BEG: user shell field
#/----------------------------------------------------------------------------
#| User shell script can be implemented at this field.

__VC_DURATION=5

# retrieve topology callback function
# param: $1 alsa params
function retrieved_param
{
    local params=$1
    [[ -z $params ]] && return 1

    local duration=60
    local dev=$(dict_value $params 'dev')
    local channel=$(dict_value $params 'channel')
    local rate=$(dict_value $params 'rate')
    local fmt=$(dict_value $params 'fmt')
    local pid

    local audioSrcRoot=$(get_customized_option "audio_src")
    local type=$(dict_value $params 'type')
    if [[ $type == playback || $type == both ]]; then
        local audioSrc=$audioSrcRoot/raw/test-c${channel}-r${rate}-f${fmt}-d${duration}.raw
        aplay -D $dev --dump-hw-params -c $channel -r $rate -f $fmt -d $__VC_DURATION $audioSrc & pid=$!
    fi

    # TODO: need set correct the PGA pipeline for PCM.
    amixer cset name="PGA1.0 Master Playback Volume 1" $((RANDOM % 100))
    [[ -n $pid && -d /proc/$pid ]] && kill -9 $pid
}
#\----------------------------------------------------------------------------
#| END: user shell field
##############################################################################

##############################################################################
#| BEG: override the functions
#/----------------------------------------------------------------------------
function __case_passed
{
    echo -n #__OCCUPY_LINE_DELETE_ME__ case passed post response
}

function __case_failed
{
    echo -n #__OCCUPY_LINE_DELETE_ME__ case failed post response
}

function __case_blocked
{
    echo -n #__OCCUPY_LINE_DELETE_ME__ case blocked post response
}

function __execute
{
    logw "TODO: Dummy the volume change, need detect correct pipeline from tplg binary."
    #retrieve_tplg retrieved_param
    return $?
}
#\----------------------------------------------------------------------------
#| END: override the functions
##############################################################################

#/----------------------------------------------------------------------------
#| manual executable entrance
#/----------------------------------------------------------------------------
[[ -z $__auto__ ]] && {
    __execute $*
    [[ $? -ne 0 ]] && __case_failed || __case_passed
}
#\----------------------------------------------------------------------------

#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

##############################################################################
#| BEG: user shell field
#/----------------------------------------------------------------------------
#| User shell script can be implemented at this field.
__PB_3M_DURATION=180

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

    # e.g.: test-c2-r48000-fs32_le-d600.raw
    local audioSrcRoot=$(get_customized_option "audio_src")
    local type=$(dict_value $params 'type')
    if [[ $type == playback || $type == both ]]; then
        local audioSrc=$audioSrcRoot/raw/test-c${channel}-r${rate}-f${fmt}-d${duration}.raw
        [[ ! -f $audioSrc ]] && audioSrc=/dev/zero
        aplay -D $dev --dump-hw-params -r $rate -c $channel -f $fmt -d $__PB_3M_DURATION $audioSrc
    else
        logi "Skip Device [$dev]($type)..."
        return 199
    fi
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
    retrieve_tplg retrieved_param
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

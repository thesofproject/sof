#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

##############################################################################
#| BEG: user shell field
#/----------------------------------------------------------------------------
#| User shell script can be implemented at this field.

# retrieve topology callback function
# param: $1 alsa params
function retrieved_param
{
    local param=$1
    [[ -z $param ]] && return 1

    local type=$(dict_value $param 'type')
    local dev=$(dict_value $param 'dev')
    local rate=$(dict_value $param 'rate')
    local channel=$(dict_value $param 'channel')
    local fmt=$(dict_value $param 'fmt')

    if [[ $type == playback || $type == both ]]; then
        check_speaker $dev $rate $fmt $channel 3
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
    # cannot execute this case speratedly.
    [[ -z $__auto__ ]] && return 0

    retrieve_tplg retrieved_param
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

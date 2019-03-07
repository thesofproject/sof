#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

##############################################################################
#| BEG: user shell field
#/----------------------------------------------------------------------------
__AP_DURATION=5

# retrieve topology callback function
# param: $1 alsa params
function retrieved_param
{
    local params=$1
    [[ -z $params ]] && return 1

    local dev=$(dict_value $params 'dev')
    local channel=$(dict_value $params 'channel')
    local rate=$(dict_value $params 'rate')
    local fmt=$(dict_value $params 'fmt')
    local type=$(dict_value $params 'type')
    if [[ $type == playback || $type == both ]]; then
        aplay -D $dev --dump-hw-params -r $rate -c $channel -f $fmt -d $__AP_DURATION /dev/zero
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

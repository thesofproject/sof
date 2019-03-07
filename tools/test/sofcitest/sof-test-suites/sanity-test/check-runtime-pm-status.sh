#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

##############################################################################
#| BEG: user shell field
#/----------------------------------------------------------------------------
#| User shell script can be implemented at this field.

__RUNTIME_STAT="/sys/bus/pci/devices/0000:`lspci | grep -i audio | cut -d ' ' -f 1`/power/runtime_status"

# retrieve topology callback function
# param: $1 alsa params
function retrieved_param
{
    local param=$1
    [[ -z $param ]] && return 1

    local pid
    local result
    local type=$(dict_value $param 'type')
    local dev=$(dict_value $param 'dev')
    local rate=$(dict_value $param 'rate')
    local channel=$(dict_value $param 'channel')
    local fmt=$(dict_value $param 'fmt')

    if [[ $type == playback || $type == both ]]; then
        # playback device - check status
        aplay -D $dev -r $rate -c $channel -f $fmt /dev/zero & pid=$!
        sleep 0.5
        [[ -d /proc/$pid ]] && result=`cat $__RUNTIME_STAT`
        logi "start playback device - check status: $result"
        if [[ $result == active ]]; then
            # stop playback device - check status again
            kill -9 $pid
            sleep 3
            result=`cat $__RUNTIME_STAT`
            logi "stop playback device - check status: $result"
            if [[ $result == suspended ]]; then
                return 0
            fi
        fi

        return 1
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
    [[ ! -f $__RUNTIME_STAT ]] && return 1
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

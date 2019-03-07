#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

##############################################################################
#| BEG: user shell field
#/----------------------------------------------------------------------------
#| User shell script can be implemented at this field.
__RELOAD_COUNT=100
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
    local caseInfo=$1
    local logDir=$(dict_value "$caseInfo" "logdir" "/tmp")
    kmod_reload $__RELOAD_COUNT 1 $logDir
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

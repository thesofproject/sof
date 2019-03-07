#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

##############################################################################
#| BEG: override the functions
#/----------------------------------------------------------------------------
function __case_passed
{
    __OCCUPY_LINE_DELETE_ME__ #case passed post response
}

function __case_failed
{
    __OCCUPY_LINE_DELETE_ME__ #case failed post response
}

function __case_blocked
{
    __OCCUPY_LINE_DELETE_ME__ #case blocked post response
}

function __execute
{
    # Verify DSP firmware is presence
    [[ ! -f /lib/firmware/intel/sof/sof-$_PLATFORM.ri ]] && return 1

    # TODO: list all firmware that need be checked.
    return 0
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


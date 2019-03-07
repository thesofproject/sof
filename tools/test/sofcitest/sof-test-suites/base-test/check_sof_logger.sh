#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

__TEMP_CHECK_LOG=/tmp/sof-logger.log
__SOF_LOGGER=sof-logger
__SOF_LDC_FILE=/etc/sof/sof-$_PLATFORM.ldc

function __case_passed
{
    rm -rf $__TEMP_CHECK_LOG
}

function __case_failed
{
    rm -rf $__TEMP_CHECK_LOG
}

function __execute
{
    local errInfo=`echo $sudopwd | sudo -S $__SOF_LOGGER -t -l $__SOF_LDC_FILE -o $__TEMP_CHECK_LOG 2>&1 & sleep 2; echo $sudopwd | sudo -S pkill -9 $__SOF_LOGGER`

    [[ -n $errInfo ]] && return 1

    # get size of trace log$
    size=`du -k $__TEMP_CHECK_LOG | awk '{print $1}'`
    [[ $size -gt 1 ]] && return 0 || return 1
}

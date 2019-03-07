#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

__S3_LOOP_CNT=1000
__S3_LOOP_INTERVAL=5

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
    pm_s3_loop $__S3_LOOP_CNT $__S3_LOOP_INTERVAL $params
}

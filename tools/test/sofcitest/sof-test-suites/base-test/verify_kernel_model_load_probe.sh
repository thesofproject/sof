#!/bin/bash
PASSED_INFO=""
FAILED_INFO="SOF Modules were not loaded."

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
    lsmod | grep -i snd_soc > /dev/null
    return $?
}

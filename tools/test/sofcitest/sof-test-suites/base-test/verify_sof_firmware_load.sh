#!/bin/bash
PASSED_INFO=""
FAILED_INFO="DSP firmware were not loaded."

function __case_passed
{
    __OCCUPY_LINE_DELETE_ME__ #case passed post response
}

function __case_failed
{
    __OCCUPY_LINE_DELETE_ME__ #case passed post response
}

function __execute
{
    # TODO: Need a version file to track all components version: include the sof, kernel, tplg.
    dmesg | grep "] sof-audio.*version"
    if [[ $? -eq 0 ]]; then
        dmesg | grep 'firmware boot complete'
        local ret=$?
        [[ $ret -ne 0 ]] && echo "[WARNING]: cannot find the sof 'firmware boot complete'"
        return $ret
    else
        echo "[WARNING]: cannot find the sof audio version"
        return 1
    fi
}

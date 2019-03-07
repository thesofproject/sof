#!/bin/bash
PASSED_INFO=""
FAILED_INFO=""

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
    [[ -z $_TPLG ]] && return 1

    local tplgDat=$(tplgreader -i $_TPLG -j)
    [[ -z $tplgDat ]] && return 1 || return 0
}

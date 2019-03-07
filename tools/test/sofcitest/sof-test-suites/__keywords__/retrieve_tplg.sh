#!/bin/bash

define __STEREO=2

# retreive the topology params, and invoke the callback function
# param: $1 callback function
function retrieve_tplg
{
    local func_cb=$1
    [[ -z "$func_cb" ]] && {
        logw "No callback function input for topology params retrieving. skip..."
        return 1
    }

    [[ -z $_TPLG ]] && return 1

    local tplgDat=($(tplgreader -i $_TPLG -j | sed 's/ /_/g;'))
    [[ ${#tplgDat[@]} -eq 0 ]] && return 1

    local cnt=0
    local passedCnt=0
    local failedCnt=0
    for tplg in ${tplgDat[@]}; do
        cnt=$((cnt + 1))
        local type=$(dict_value $tplg 'TYPE')
        local devId=$(dict_value $tplg 'ID')
        local dev="hw:$sofcard,$devId"
        local rate=$(dict_value $tplg 'RATE_MIN')
        local fmt=$(dict_value $tplg 'FMT')
        [[ $fmt == ${fmt//_/} ]] && fmt=${fmt:0:3}_${fmt:3:2}
        local params="{type=$type;dev=$dev;rate=$rate;channel=$__STEREO;fmt=$fmt;}"
        $func_cb "$params"
        local ret=$?

        if [[ $ret -eq 0 ]]; then
            logi "Case PASSED via PCM: "$(dict_value $tplg 'PCM')
            passedCnt=$((passedCnt + 1))
        elif [[ $ret -eq 199 ]]; then
            logi "Ignore Case via PCM: "$(dict_value $tplg 'PCM')
        else
            logi "Case FAILED via PCM: "$(dict_value $tplg 'PCM')
            failedCnt=$((failedCnt + 1))
        fi
    done
    [[ $passedCnt -eq 0 && $failedCnt -gt 0 ]] && return 1 || return 0
}

declare -fx retrieve_tplg

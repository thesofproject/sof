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

function __execute
{
    [[ -z $_TPLG ]] && return 1

    local tplgDat=($(tplgreader -i $_TPLG -j | sed 's/ /_/g;'))
    local tpbList=()
    local tcpList=()
    for tplg in ${tplgDat[@]}; do
        echo "toplogy item: $tplg"
        local type=$(dict_value $tplg 'TYPE')
        local id=$(dict_value $tplg 'ID')
        local name=$(dict_value $tplg 'PCM')
        name=${name// /}
        case $type in
            "both")
                tpbList=(${tpbList[@]} $id:$name)
                tcpList=(${tcpList[@]} $id:$name)
                ;;
            "playback")
                tpbList=(${tpbList[@]} $id:$name)
                ;;
            "capture")
                tcpList=(${tcpList[@]} $id:$name)
                ;;
        esac
    done

    local pbList=(`aplay -l 2>/dev/null | sed -n '/^card '$sofcard'.*device /p;' | sed 's/.*device //; s/ (.*//; s/ //; s/ /_/g;' | xargs echo`)
    local cpList=(`arecord -l 2>/dev/null | sed -n '/^card '$sofcard'.*device /p;' | sed 's/.*device //; s/ (.*//; s/ //; s/ /_/g;' | xargs echo`)

    echo "Get playback list: ${pbList[@]}"
    echo "Get capture list: ${cpList[@]}"

    if [[ ${#tpbList[@]} -ne ${#pbList[@]} || ${#tcpList[@]} -ne ${#cpList[@]} ]]; then
        echo "[WARNING]: The $_TPLG doesn't match the loaded topology!"
        return 1
    else
        local matchedCnt=0
        for i in ${tpbList[@]}; do
            for j in ${pbList[@]}; do
                if [[ $i == $j ]]; then
                    matchedCnt=$((matchedCnt + 1))
                    break
                fi
            done
        done
        if [[ ${#tpbList[@]} -ne $matchedCnt ]]; then
            echo "[WARNING]: The $_TPLG doesn't match the loaded topology! (playback)"
            return 1
        fi

        matchedCnt=0
        for i in ${tcpList[@]}; do
            for j in ${cpList[@]}; do
                if [[ $i == $j ]]; then
                    matchedCnt=$((matchedCnt + 1))
                    break
                fi
            done
        done
        if [[ ${#tcpList[@]} -ne $matchedCnt ]]; then
            echo "[WARNING]: The $_TPLG doesn't match the loaded topology! (capture)"
            return 1
        fi
    fi
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

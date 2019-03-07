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
    retrieve_tplg retrieved_param
    return $?
}

# retrieve topology callback function
# param: $1 alsa params
function retrieved_param
{
    local params=$1
    [[ -z $params ]] && return 1

    params=$(dict_update $params 'time=0.5')
    params=$(dict_update $params 'count=100')

    local type=$(dict_value $params 'type')

    if [[ "$type" == "playback" ]]; then
        let i=0
        let cnt=1000
        while [[ $i -lt $cnt ]]; do
            i=$((i + 1))
            if [[ $((i % 100)) -eq 0 ]]; then
                echo -en '#\n'
            else
                if [[ $((i % 10)) -eq 0 ]]; then
                    echo -n '# '
                else
                    echo -n '#'
                fi
            fi
            aplay -Dhw:0,0 -c 2 -r 48000 -f s16_le -d 1 /dev/zero 2>/dev/null
            if [[ $? -ne 0 ]]; then
                dmesg > $LOG_OUTPUT_ROOT/aplay_error_$i.txt
                loge "stress aplay 1000 time failed, failed at [$i] times."
                return 1
            fi
        done
    fi
    return 0
}

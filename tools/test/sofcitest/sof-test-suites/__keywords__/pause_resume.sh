#!/bin/bash

# internal: common pause resume function
# params: $1 dict of audio params
function __pause_resume_cmd
{
    local params=$1
    local alsaCmd=$(dict_value $params 'cmd' "aplay")
    local time=$(dict_value $params 'time' "0.5")
    local duration=$(dict_value $params 'duration' "10")
    local device=$(dict_value $params 'dev' "hw:0,0")
    local rate=$(dict_value $params 'rate' "48000")
    local channel=$(dict_value $params 'channel' "2")
    local fmt=$(dict_value $params 'fmt' "S32_LE")
    local dummyFile=$(dict_value $params 'file')
    local cnt=$(dict_value $params 'count' "10")
    if [[ -z $dummyFile || ! -f $dummyFile ]]; then
        [[ "$alsaCmd" == "arecord" ]] && dummyFile=/dev/null || dummyFile=/dev/zero
    fi

    expect <<!
spawn $alsaCmd -D$device -r $rate -c $channel -f $fmt -vv -i -d $duration $dummyFile
set i 0
expect {
    "Input/output" {
        send_user "error\n"
        exit 1
    }
    "*#+*" {
        sleep $time
        send " "
        if { \$i < $cnt } {
            incr i
            exp_continue
        }
        exit 0
    }
}
!
    return $?
}

# pause/resume during audio playback
# params: $1 dict - {cmd=aplay;time=0.5;duration=10;device=hw:0,0;rate=48000;channel=2;fmt=S32_LE;}
function pause_resume_playback
{
    local params=$1
    params="$(dict_update $params 'cmd=aplay')"
    __pause_resume_cmd $params
}

# pause/resume during audio capture
# params: $1 dict - {cmd=aplay;time=0.5;duration=10;device=hw:0,0;rate=48000;channel=2;fmt=S32_LE;}
function pause_resume_capture
{
    local params=$1
    params="$(dict_update $params 'cmd=arecord')"
    __pause_resume_cmd $params
}

declare -fx __pause_resume_cmd
declare -fx pause_resume_playback
declare -fx pause_resume_capture

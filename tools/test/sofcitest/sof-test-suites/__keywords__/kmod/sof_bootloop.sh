#!/bin/bash
. $PROJ_ROOT/__keywords__/kmod/common.sh
. $PROJ_ROOT/__keywords__/kmod/sof_bootone.sh

:<<'!'
Waiting for aplay is ready based on the timeout value(seconds).
  @timeout($1): The timeout value(seconds) waiting for aplay is ready.
!
function wait_aplay_ready
{
    local timeout=$1
    local elapsed_sec=0
    while [ $elapsed_sec -lt $timeout ]; do
        local ready=$(aplay -l | grep Subdevices | awk -F " " '{print $2}' \
          | awk -F "/" 'BEGIN{ready=1}{if($1!=$2){ready=0;exit}}END{print ready}')
        [ -z $ready ] && ready=0
        [ 1 -eq $ready ] && return $ready
        sleep 1
        let elapsed_sec+=1
        #echo "try $elapsed_sec"
    done
    return 0
}

function run_loop(){
    local max_loop=$1
    local ignore_error=$2
    local log_dir=$3
    local counter=0
    local platform=`get_platform`
    local codec_module=`get_codec_module`

    # The timeout value(seconds) waiting for aplay is ready.
    local timeoutWaitAplay=30

    while [ $counter -lt $max_loop ]; do
        echo "test $counter"
        wait_aplay_ready $timeoutWaitAplay
        if [ 0 -eq $? ]; then
            echo "aplay is not ready!"
            return 1
        fi

        run_boot $platform $codec_module $ignore_error
        if [ 0 -ne $? ]; then
            echo "fail to boot firmware!"
            return 1
        fi

        if [[ -z $log_dir ]]; then
            log_dir=/tmp
        else
            [[ ! -d $log_dir ]] && mkdir -p $log_dir
        fi
        dmesg > $log_dir/boot_$counter.bootlog
        let counter+=1
    done
    echo "==== boot firmware: $counter times ===="
    return 0
}

declare -fx run_loop
declare -fx wait_aplay_ready

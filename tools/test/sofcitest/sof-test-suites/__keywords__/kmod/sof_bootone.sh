#!/bin/bash
. $PROJ_ROOT/__keywords__/kmod/common.sh
. $PROJ_ROOT/__keywords__/kmod/sof_insert.sh
. $PROJ_ROOT/__keywords__/kmod/sof_remove.sh

function show_help() {
    echo -e "Usage: $0 [platform] [codec_module] [ignore_error]\n"\
      "  Supported 'platforms': byt, apl, cnl, icl or whl.\n"\
      "  Valid 'ignore_error': true, false, yes, no, y, n, 1, 0."
    exit 1
}

function run_boot(){
    local platform
    local codec_module
    local fw_boot
    local timeout
    local error

    if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
        show_help $0
    fi

    if [ $# -ge 2 ]; then
        platform=$1
        codec_module=$2
        ignore_error=${3:-"no"}
    else
        platform=`get_platform`
        codec_module=`get_codec_module`
        ignore_error=${1:-"no"}
    fi
    echo "platform=[$platform],codec_module=[$codec_module],ignore_error=[$ignore_error]"

    check_bool $ignore_error
    [ 0 -eq $? ] && show_help $0
    get_bool $ignore_error
    ignore_error=$?

    echo $sudopwd | sudo -S dmesg -C
    unload_modules
    load_modules $platform $codec_module
    sleep 1

    fw_boot=$(dmesg | grep sof-audio | grep "boot complete")
    timeout=$(dmesg | grep sof-audio | grep "ipc timed out")

    if [ 0 -eq $ignore_error ]; then
        error=$(dmesg | grep sof-audio | grep -v "DSP trace buffer overflow" | grep "error")
    fi
    if [ ! -z "$error" ] || [ -z "$fw_boot" ] || [ ! -z "$timeout" ]; then
    [ ! -d $REPORT_ROOT/boot-log ] && mkdir $REPORT_ROOT/boot-log/
        dmesg > $REPORT_ROOT/boot-log/boot_fail.txt
        echo "boot failed, see boot_fail.txt for details"
        exit 2
    else
        echo "boot success"
    fi
}

declare -fx run_boot

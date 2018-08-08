#!/bin/bash

dmesg -C
./sof_remove.sh
./sof_insert.sh
sleep 1

unset FW_BOOT
unset ERROR
FW_BOOT=$(dmesg | grep sof-audio | grep "boot complete")
ERROR=$(dmesg | grep sof-audio | grep -v "DSP trace buffer overflow" | grep "error")
TIMEOUT=$(dmesg | grep sof-audio | grep "ipc timed out")

if [ ! -z "$ERROR" ] || [ -z "$FW_BOOT" ] || [ ! -z "$TIMEOUT" ]
then
    dmesg > boot_fail.log
    echo "boot failed, see boot_fail.log for details"
    exit 1
else
    echo "boot success"
fi
       

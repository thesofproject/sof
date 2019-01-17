#!/bin/bash

dmesg -C
./sof_remove.sh

# exit if remove failed
ERROR=$(dmesg | grep sof-audio | grep "error")
if [ ! -z "$ERROR" ]
then
    dmesg > boot_fail.log
    echo "sof_remove failed, see boot_fail.log for details"
    exit 1
fi

unset ERROR

./sof_insert.sh
sleep 1

unset FW_BOOT
unset ERROR
FW_BOOT=$(dmesg | grep sof-audio | grep "boot complete")
ERROR=$(dmesg | grep sof-audio | grep -v "DSP trace buffer overflow" | grep "error")
TIMEOUT=$(dmesg | grep sof-audio | grep "ipc timed out")

# exit if insert failed
if [ ! -z "$ERROR" ] || [ -z "$FW_BOOT" ] || [ ! -z "$TIMEOUT" ]
then
    dmesg > boot_fail.log
    echo "sof_insert failed, see boot_fail.log for details"
    exit 1
else
    echo "boot success"
fi
       

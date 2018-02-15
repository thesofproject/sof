#!/bin/bash

dmesg -C
./sof_remove.sh
./sof_insert.sh
sleep 1

unset FW_BOOT
unset ERROR
FW_BOOT=$(dmesg | grep sof-audio | grep "boot complete")
ERROR=$(dmesg | grep sof-audio | grep "error")

if [ ! -z "$ERROR" ] || [ -z "$FW_BOOT" ]
then
   echo "boot failed"
else
    echo "boot success"
fi
       

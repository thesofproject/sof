#!/bin/bash

set -e

MAXLOOPS=100
COUNTER=0

while [ $COUNTER -lt $MAXLOOPS ]; do
    echo "test $COUNTER"
    ./sof_bootone.sh
    dmesg > boot_$COUNTER.bootlog
    let COUNTER+=1
done

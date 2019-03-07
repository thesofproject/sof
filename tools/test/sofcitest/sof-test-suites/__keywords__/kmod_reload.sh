#!/bin/bash

. $PROJ_ROOT/__keywords__/kmod/sof_bootloop.sh

function kmod_reload
{
    echo "PROJ_ROOT:"$PROJ_ROOT
    run_loop $@
}

declare -fx kmod_reload

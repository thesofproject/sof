#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation. All rights reserved.

# set -x

set -e

SOF_TOP="$(cd "$(dirname "$0")"/.. && /bin/pwd)"

main()
{
    # Log container versions
    for rep in zephyrprojectrtos/zephyr-build \
               ghcr.io/zephyrproject-rtos/zephyr-build ; do
        docker images --digests "$rep"
    done

    if tty --quiet; then
        SOF_DOCKER_RUN="$SOF_DOCKER_RUN --tty"
    fi

    cd "$SOF_TOP"

    set -x

    docker run -v "$(west topdir)":/zep_workspace \
           --workdir /zep_workspace \
           $SOF_DOCKER_RUN \
           ghcr.io/zephyrproject-rtos/zephyr-build:latest \
           "$@"
}

main "$@"

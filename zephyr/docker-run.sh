#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation. All rights reserved.

# set -x

# Tip: if you're not really sure what image version you're using,
# invoke this script with sample commands like these:
#
#  ./sof/zephyr/docker-run.sh  ls -l /opt/toolchains/
#  ./sof/zephyr/docker-run.sh  /opt/sparse/bin/sparse --version
#  ./sof/zephyr/docker-run.sh  /bin/bash
#
#   etc.
# https://github.com/zephyrproject-rtos/docker-image/commits/master
#
# "latest" is just the default tag used when you don't specify any. To
# use this script with a image other than the one officially tagged
# "latest", simply overwrite the official tag. Example:
#
#   docker tag ghcr.io/zephyrproject-rtos/zephyr-build:v0.26.6
#              ghcr.io/zephyrproject-rtos/zephyr-build:latest
#
# "latest" is just a regular tag like "v0.26.6", it may or many not name
# the most recent image.
#
# To automatically restore the official "latest" tag, just delete it:
#
#   docker image rm ghcr.io/zephyrproject-rtos/zephyr-build:latest
#
# (The actual image stays in the docker cache, only the tag is deleted)

set -e

SOF_TOP="$(cd "$(dirname "$0")"/.. && /bin/pwd)"

main()
{
    # Log container versions
    # Header
    docker images --digests | grep REPOSITORY
    docker images --digests | grep -i zephyr | sort

    if tty --quiet; then
        SOF_DOCKER_RUN="$SOF_DOCKER_RUN --tty"
    fi

    cd "$SOF_TOP"

    run_command lsb_release -a
    set -x
    run_command "$@"
}

run_command()
{
    # zephyr-build:v0.27.4 has /opt/toolchains/zephyr-sdk-0.17.0
    # https://hub.docker.com/r/zephyrprojectrtos/zephyr-build/tags
    #
    # Keep this SDK version identical to the one in
    #    .github/workflows/zephyr.yml#Windows
    docker run -i -v "$(west topdir)":/zep_workspace \
           --workdir /zep_workspace \
           $SOF_DOCKER_RUN \
           --env REAL_CC --env http_proxy --env https_proxy \
           ghcr.io/zephyrproject-rtos/zephyr-build:v0.27.4 \
           ./sof/scripts/sudo-cwd.sh "$@"
}

main "$@"

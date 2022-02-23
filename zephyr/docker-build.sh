#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# "All problems can be solved by another level of indirection"
# Ideally, this script would not be needed.
#
# Minor adjustments to the docker image provided by the Zephyr project.

set -e
set -x

unset ZEPHYR_BASE

# Make sure we're in the right place; chgrp -R below.
test -e ./scripts/xtensa-build-zephyr.py

# As of container version 0.18.4,
# https://github.com/zephyrproject-rtos/docker-image/blob/master/Dockerfile
# installs two SDKs: ZSDK_VERSION=0.12.4 and ZSDK_ALT_VERSION=0.13.1
# ZEPHYR_SDK_INSTALL_DIR points at ZSDK_VERSION but we want the latest.
unset ZEPHYR_SDK_INSTALL_DIR

# Zephyr's CMake does not look in /opt but it searches $HOME
ls -ld /opt/toolchains/zephyr-sdk-*
ln -s  /opt/toolchains/zephyr-sdk-*  ~/

if test -e zephyrproject; then
    ./scripts/xtensa-build-zephyr.py  "$@"
else # -c(lone) with west init etc.
    # Matches docker.io/zephyrprojectrtos/zephyr-build:latest gid
    ls -ln | head
    stat .
    sudo chgrp -R 1000 .
    sudo chmod -R g+rwX .
    ./scripts/xtensa-build-zephyr.py -c "$@"
fi

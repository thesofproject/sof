#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# "All problems can be solved by another level of indirection"
# Ideally, this script would not be needed.

set -e
set -x

# export http_proxy=...
# export https_proxy=...

unset ZEPHYR_BASE

sudo --preserve-env=http_proxy,https_proxy apt-get update
sudo --preserve-env=http_proxy,https_proxy apt-get -y install libssl-dev

# Make sure we're in the right place; chgrp -R below.
test -e ./scripts/xtensa-build-zephyr.sh

if test -e zephyrproject; then
    ./scripts/xtensa-build-zephyr.sh -a
else
    # Matches docker.io/zephyrprojectrtos/zephyr-build:latest gid
    ls -ln | head
    stat .
    sudo chgrp -R 1000 .
    sudo chmod -R g+rwX .
    ./scripts/xtensa-build-zephyr.sh -a -c
fi

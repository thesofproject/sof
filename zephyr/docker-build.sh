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
test -e ./scripts/xtensa-build-zephyr.sh

sudo apt-get update
sudo apt-get -y install tree

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

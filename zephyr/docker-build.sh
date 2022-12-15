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

# Make sure we're in the right place
test -e ./sof/scripts/xtensa-build-zephyr.py

# See .github/workflows/zephyr.yml
# /opt/sparse is the current location in the zephyr-build image.
# Give any sparse in the workspace precedence.
PATH="$(pwd)"/sparse:/opt/sparse/bin:"$PATH"
command -V sparse  || true
: REAL_CC="$REAL_CC"


# TODO: move all code to a function
# https://github.com/thesofproject/sof-test/issues/740

# As of container version 0.18.4,
# https://github.com/zephyrproject-rtos/docker-image/blob/master/Dockerfile
# installs two SDKs: ZSDK_VERSION=0.12.4 and ZSDK_ALT_VERSION=0.13.1
# ZEPHYR_SDK_INSTALL_DIR points at ZSDK_VERSION but we want the latest.
unset ZEPHYR_SDK_INSTALL_DIR

# Zephyr's CMake does not look in /opt but it searches $HOME
ls -ld /opt/toolchains/zephyr-sdk-*
ln -s  /opt/toolchains/zephyr-sdk-*  ~/ || true

# CMake v3.21 changed the order object files are passed to the linker.
# This makes builds before that version not reproducible.
# To save time don't install if recent enough.
pip install 'cmake>=3.21'
PATH="$HOME"/.local/bin:"$PATH"

if test -e .west || test -e zephyr; then
    init_update=''
else
    init_update='-u'
fi

# To investigate what went wrong enable the trailing comment.
# This cannot be enabled by default for automation reasons.
./sof/scripts/xtensa-build-zephyr.py $init_update --no-interactive "$@" # || /bin/bash

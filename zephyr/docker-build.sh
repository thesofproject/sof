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


# Zephyr's CI has complex use cases and considers FindZephyr-sdk.cmake
# autodetection to be too unpredictable and risky. So in these docker images,
# they install the SDK in /opt/toolchains/ to *avoid autodetection on
# purpose*. By design, this forces Zephyr CI to hardcode ZEPHYR_SDK_INSTALL_DIR
# everywhere (see for instance
# https://github.com/zephyrproject-rtos/zephyr/commit/d6329386e9). But we
# already hardcode the docker image version (currently in
# zephyr/docker-run.sh). Our use case is simple, immutable and safe and we
# don't need to version _twice_ what is for us the same thing. Creating these
# symlinks restores the convenient autodetection so this script can be easily
# re-used with any docker image version.
ls -ld /opt/toolchains/zephyr-sdk-*
ln -s  /opt/toolchains/zephyr-sdk-*  ~/ || true

# Make sure FindZephyr-sdk.cmake returns the highest version.  For instance,
# the old container version 0.18.4 had two SDKs installed: ZSDK_VERSION=0.12.4
# and ZSDK_ALT_VERSION=0.13.1. Back then, ZEPHYR_SDK_INSTALL_DIR pointed at the
# older one!
unset ZEPHYR_SDK_INSTALL_DIR

# CMake v3.21 changed the order object files are passed to the linker.
# This makes builds before that version not reproducible.
# To save time don't install if recent enough.

if test -e .west || test -e zephyr; then
    init_update=''
else
    init_update='-u'
fi

# To investigate what went wrong enable the trailing comment.
# This cannot be enabled by default for automation reasons.
./sof/scripts/xtensa-build-zephyr.py $init_update --no-interactive "$@" # || /bin/bash

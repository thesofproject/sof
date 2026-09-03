#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Google LLC.  All rights reserved.
# Author: Andy Ross <andyross@google.com>
set -ex

PLATFORMS="$*"
if [ -z "$PLATFORMS" ]; then
    PLATFORMS="mt8195 mt8188 mt8186"
fi

SOF=`cd ../../../..; /bin/pwd`

for p in $PLATFORMS; do

    SRCS="$SOF/src/platform/$p/lib/dai.c $SOF/src/platform/$p/afe-platform.c"

    INCS="-I$SOF/src/include -I$SOF/src/platform/posix/include -I$SOF/posix/include"
    INCS="$INCS -I$SOF/src/arch/host/include -I$SOF/src/platform/$p/include/platform"
    INCS="$INCS -I$SOF/src/platform/$p/include"

    DEFS="-DRELATIVE_FILE=\"mt-dai-gen.c\" -DCONFIG_CORE_COUNT=1 -DCONFIG_IPC_MAJOR_3=1"

    touch uuid-registry.h
    INCS="$INCS -I."

    gcc -g -Wall -Werror -m32 -o mt-dai-gen mt-dai-gen.c $SRCS $INCS $DEFS

    ./mt-dai-gen > afe-${p}.dts
done

#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation. All rights reserved.


set -e

MAJOR=`grep '#define SOF_ABI_MAJOR ' $1/src/include/kernel/abi.h | grep -E ".[[:digit:]]$" -o`
MINOR=`grep '#define SOF_ABI_MINOR ' $1/src/include/kernel/abi.h | grep -E ".[[:digit:]]$" -o`
PATCH=`grep '#define SOF_ABI_PATCH ' $1/src/include/kernel/abi.h | grep -E ".[[:digit:]]$" -o`
MAJOR_SHIFT=`grep '#define SOF_ABI_MAJOR_SHIFT'\
	$1/src/include/kernel/abi.h | grep -E ".[[:digit:]]$" -o`
MINOR_SHIFT=`grep '#define SOF_ABI_MINOR_SHIFT'\
	$1/src/include/kernel/abi.h | grep -E ".[[:digit:]]$" -o`

major_val=$(($MAJOR << $MAJOR_SHIFT))
minor_val=$(($MINOR << $MINOR_SHIFT))
abi_version_3_8=$((3<<$MAJOR_SHIFT | 8<<$MINOR_SHIFT))
abi_version=$(($major_val | $minor_val))
abi_version_3_9_or_greater=$(($abi_version > $abi_version_3_8))
abi_version_3_17=$((3<<$MAJOR_SHIFT | 17<<$MINOR_SHIFT))
abi_version_3_17_or_greater=$(($abi_version >= $abi_version_3_17))

printf "define(\`SOF_ABI_MAJOR', \`0x%02x')\n" $MAJOR > abi.h
printf "define(\`SOF_ABI_MINOR', \`0x%02x')\n" $MINOR >> abi.h
printf "define(\`SOF_ABI_PATCH', \`0x%02x')\n" $PATCH >> abi.h
printf "define(\`SOF_ABI_VERSION', \`0x%x')\n" $abi_version >> abi.h
printf "define(\`SOF_ABI_VERSION_3_9_OR_GRT', \`%d')\n"\
	$abi_version_3_9_or_greater >> abi.h
printf "define(\`SOF_ABI_VERSION_3_17_OR_GRT', \`%d')\n"\
	$abi_version_3_17_or_greater >> abi.h

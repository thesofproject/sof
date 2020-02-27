#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation. All rights reserved.
# Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

# Utility for filtering flags passed to the compiler.

set -e

out_args=()

for arg in "$@"; do
	for target in $PROXY_REMOVE_FLAGS; do
		if [[ $arg = $target ]]; then
			continue 2
		fi
	done
	out_args+=("$arg")
done


$PROXY_CC "${out_args[@]}"

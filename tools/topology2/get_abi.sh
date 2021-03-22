#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation. All rights reserved.
set -e

ABI_MAJOR=$(awk '/^ *# *define *SOF_ABI_MAJOR / { print $3 }' $1/src/include/kernel/abi.h)
ABI_MINOR=$(awk '/^ *# *define *SOF_ABI_MINOR / { print $3 }' $1/src/include/kernel/abi.h)
ABI_PATCH=$(awk '/^ *# *define *SOF_ABI_PATCH / { print $3 }' $1/src/include/kernel/abi.h)

cat $2
cat <<EOF_HEADER

Object.manifest."0" {
	name	"sof_manifest"
	Object.data."0" {
		name	"sof_manifest"
EOF_HEADER
printf '\t\tbytes\t"0x%02x,' "$ABI_MAJOR"
printf "0x%02x," "$ABI_MINOR"
printf '0x%02x\"\n' "$ABI_PATCH"
printf "\t}\n"
printf "}"

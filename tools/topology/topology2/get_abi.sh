#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation. All rights reserved.
set -e

ABI_MAJOR=$(awk '/^ *# *define *SOF_ABI_MAJOR / { print $3 }' $1/src/include/kernel/abi.h)
ABI_MINOR=$(awk '/^ *# *define *SOF_ABI_MINOR / { print $3 }' $1/src/include/kernel/abi.h)
ABI_PATCH=$(awk '/^ *# *define *SOF_ABI_PATCH / { print $3 }' $1/src/include/kernel/abi.h)

cat <<EOF_HEADER
Object.Base.manifest."sof_manifest" {
	Object.Base.data."SOF ABI" {
EOF_HEADER

print_1byte()
{
	printf '0x%02x' "$(("$1" & 0xff))"
}

print_2bytes()
{
	printf '0x%02x,0x%02x' "$(("$1" & 0xff))" "$((("$1" & 0xff00) >> 8))"
}

printf '\t\tbytes\t\"'
for vers in $ABI_MAJOR $ABI_MINOR; do
	case "$2" in
		ipc3) print_1byte "$vers" ;;
		ipc4) print_2bytes "$vers" ;;
	esac
	printf ','
done

case "$2" in
	ipc3) print_1byte "$ABI_PATCH" ;;
	ipc4) print_2bytes "$ABI_PATCH" ;;
esac

printf '\"\n\t}\n}'

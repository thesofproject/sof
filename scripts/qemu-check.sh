#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.
set -e

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl icl skl kbl cnl imx8 imx8x imx8m imx8ulp)
READY_MSG="6c 00 00 00 00 00 00 70"

SOF_DIR=$(cd "$(dirname "$0")" && cd .. && pwd)

: "${SOF_BUILDS:=${SOF_DIR}}"

rm -f dump-*.txt

die()
{
	>&2 printf '%s ERROR: ' "$0"
	# We want die() to be usable exactly like printf
	# shellcheck disable=SC2059
	>&2 printf "$@"
	exit 1
}

print_usage()
{
	cat <<EOF
usage: qemu-check.sh [ platform(s) ]
	Supported platforms are ${SUPPORTED_PLATFORMS[*]}
	Runs all supported platforms by default.
EOF
}

find_qemu_xtensa()
{
	local xhs=xtensa-host.sh
	for d in . ../qemu* qemu*; do
		if test -e "$d"/$xhs; then
			printf '%s' "$d"/$xhs;
			return
		fi
	done
	die '%s not found\n' $xhs
}


while getopts "" OPTION; do
	case "$OPTION" in
		*) print_usage; exit 1 ;;
	esac
done
shift $((OPTIND-1))

PLATFORMS=()
if [ "$#" -eq 0 ]; then
	PLATFORMS=("${SUPPORTED_PLATFORMS[@]}")
else
	for arg in "$@"; do
		platform=unknown
		for sp in "${SUPPORTED_PLATFORMS[@]}"; do
			if [ x"$sp" = x"$arg" ]; then
				PLATFORMS=("${PLATFORMS[@]}" "$sp")
				platform=$sp
				shift
				break
			fi
		done
		if [ "$platform" = "unknown" ]; then
			echo "Error: Unknown platform specified: $arg"
			echo "Supported platforms: ${SUPPORTED_PLATFORMS[*]}"
			exit 1
		fi
	done
fi

for platform in "${PLATFORMS[@]}"
do
	FWNAME="sof-$platform.ri"
	PLATFORM=$platform
	# reset variable to avoid issue in random order
	ROM=''
	OUTBOX_OFFSET=''

	has_rom=false
	case "$platform" in
		byt)
			READY_IPC="00 88 02 70 00 00 00 80"
			SHM_IPC_REG=qemu-bridge-shim-io
			SHM_MBOX=qemu-bridge-mbox-io
			;;
		cht)
			READY_IPC="00 88 02 70 00 00 00 80"
			SHM_IPC_REG=qemu-bridge-shim-io
			SHM_MBOX=qemu-bridge-mbox-io
			;;
		bdw)
			READY_IPC="00 3c 01 80"
			OUTBOX_OFFSET="9e000"
			SHM_IPC_REG=qemu-bridge-shim-io
			SHM_MBOX=qemu-bridge-dram-mem
			;;
		hsw)
			READY_IPC="00 fc 00 80"
			OUTBOX_OFFSET="7e000"
			SHM_IPC_REG=qemu-bridge-shim-io
			SHM_MBOX=qemu-bridge-dram-mem
			;;
		apl)
			READY_IPC="00 00 00 f0"
			SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
			OUTBOX_OFFSET="7000"
			SHM_MBOX=qemu-bridge-hp-sram-mem
			has_rom=true
			;;
		skl)
			READY_IPC="00 00 00 f0"
			SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
			OUTBOX_OFFSET="7000"
			SHM_MBOX=qemu-bridge-hp-sram-mem
			has_rom=true
			;;
		kbl)
			READY_IPC="00 00 00 f0"
			SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
			OUTBOX_OFFSET="7000"
			SHM_MBOX=qemu-bridge-hp-sram-mem
			has_rom=true
			;;
		cnl)
			READY_IPC="00 00 00 f0"
			SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
			OUTBOX_OFFSET="5000"
			SHM_MBOX=qemu-bridge-hp-sram-mem
			has_rom=true
			;;
		icl)
			READY_IPC="00 00 00 f0"
			SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
			OUTBOX_OFFSET="5000"
			SHM_MBOX=qemu-bridge-hp-sram-mem
			has_rom=true
			;;
		imx8 | imx8x | imx8m | imx8ulp)
			READY_IPC="00 00 00 00 00 00 04 c0"
			SHM_IPC_REG=qemu-bridge-mu-io
			SHM_MBOX=qemu-bridge-mbox-io
			;;
	esac

	if $has_rom; then
		ROM=(-r "${SOF_BUILDS}/build_${platform}_gcc/src/arch/xtensa/rom-$platform.bin")
	fi

        xtensa_host_sh=$(find_qemu_xtensa)

	${xtensa_host_sh} "$PLATFORM" -k \
	   "${SOF_BUILDS}"/build_"${platform}"_gcc/src/arch/xtensa/"$FWNAME" \
                "${ROM[@]}" \
		-o 2.0 "${SOF_BUILDS}"/dump-"$platform".txt || true # timeout
	# dump log into sof.git incase running in docker

	# use regular expression to match the SHM IPC REG file name
	SHM_IPC_REG_FILE=$(ls /dev/shm/ | grep -E $SHM_IPC_REG)

	# check if ready ipc header is in the ipc regs
	IPC_REG=$(hexdump -C /dev/shm/"$SHM_IPC_REG_FILE" |
		      grep "$READY_IPC") || true

	# check if ready ipc message is in the mbox
	IPC_MSG=$(hexdump -C /dev/shm/$SHM_MBOX | grep -A 4 "$READY_MSG" |
		      grep -A 4 "$OUTBOX_OFFSET") || true

	if [ "$IPC_REG" ]; then
		echo "ipc reg dump:"
		# directly output the log to be nice to look
		hexdump -C /dev/shm/"$SHM_IPC_REG_FILE" | grep "$READY_IPC"
		if [ "$IPC_MSG" ]; then
			echo "ipc message dump:"
			# directly output the log to be nice to look
			hexdump -C /dev/shm/$SHM_MBOX | grep -A 4 "$READY_MSG" | grep -A 4 "$OUTBOX_OFFSET"
		else
			echo "Error mailbox failed"
		fi
	else
		echo "Error ipc reg failed"
	fi

	if [[ "$IPC_REG" && "$IPC_MSG" ]]; then
		echo "Boot success";
	else
		echo "Error boot failed"
		tail -n 50 "${SOF_BUILDS}"/dump-"$platform".txt
		exit 2;
	fi
done

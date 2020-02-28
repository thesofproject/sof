#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl icl skl kbl cnl imx8 imx8x)
READY_MSG="6c 00 00 00 00 00 00 70"

rm -f dump-*.txt

if [ "$#" -eq 0 ]
then
	PLATFORMS=${SUPPORTED_PLATFORMS[@]}
else
	for args in $@
	do
		for i in ${SUPPORTED_PLATFORMS[@]}
		do
			if [ $i == $args ]
			then
				PLATFORMS+=$i" "
			fi
		done
	done
fi

# check target platform(s) have been passed in
if [ ${#PLATFORMS[@]} -eq 0 ];
then
	echo "Error: No platforms specified. Supported are: ${SUPPORTED_PLATFORMS[@]}"
	exit 1
fi

for j in ${PLATFORMS[@]}
do
	FWNAME="sof-$j.ri"
	PLATFORM=$j
	# reset variable to avoid issue in random order
	ROM=''
	OUTBOX_OFFSET=''
	if [ $j == "byt" ]
	then
		READY_IPC="00 88 02 70 00 00 00 80"
		SHM_IPC_REG=qemu-bridge-shim-io
		SHM_MBOX=qemu-bridge-mbox-io
	fi
	if [ $j == "cht" ]
	then
		READY_IPC="00 88 02 70 00 00 00 80"
		SHM_IPC_REG=qemu-bridge-shim-io
		SHM_MBOX=qemu-bridge-mbox-io
	fi
	if [ $j == "bdw" ]
	then
		READY_IPC="00 3c 01 80"
		OUTBOX_OFFSET="9e000"
		SHM_IPC_REG=qemu-bridge-shim-io
		SHM_MBOX=qemu-bridge-dram-mem
	fi
	if [ $j == "hsw" ]
	then
		READY_IPC="00 fc 00 80"
		OUTBOX_OFFSET="7e000"
		SHM_IPC_REG=qemu-bridge-shim-io
		SHM_MBOX=qemu-bridge-dram-mem
	fi
	if [ $j == "apl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
		OUTBOX_OFFSET="7000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_${j}_gcc/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "skl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
		OUTBOX_OFFSET="7000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_${j}_gcc/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "kbl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
		OUTBOX_OFFSET="7000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_${j}_gcc/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "cnl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
		OUTBOX_OFFSET="5000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_${j}_gcc/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "icl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG="qemu-bridge-ipc(|-dsp)-io"
		OUTBOX_OFFSET="5000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_${j}_gcc/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "imx8" ] || [ $j == "imx8x" ]
	then
		READY_IPC="00 00 00 00 00 00 04 c0"
		SHM_IPC_REG=qemu-bridge-mu-io
		SHM_MBOX=qemu-bridge-mbox-io
	fi

	./xtensa-host.sh $PLATFORM -k ../sof.git/build_${j}_gcc/src/arch/xtensa/$FWNAME $ROM -o 2.0 ../sof.git/dump-$j.txt
	# dump log into sof.git incase running in docker

	# use regular expression to match the SHM IPC REG file name
	SHM_IPC_REG_FILE=$(ls /dev/shm/ | grep -E $SHM_IPC_REG)

	# check if ready ipc header is in the ipc regs
	IPC_REG=$(hexdump -C /dev/shm/$SHM_IPC_REG_FILE | grep "$READY_IPC")
	# check if ready ipc message is in the mbox
	IPC_MSG=$(hexdump -C /dev/shm/$SHM_MBOX | grep -A 4 "$READY_MSG" | grep -A 4 "$OUTBOX_OFFSET")

	if [ "$IPC_REG" ]; then
		echo "ipc reg dump:"
		# directly output the log to be nice to look
		hexdump -C /dev/shm/$SHM_IPC_REG_FILE | grep "$READY_IPC"
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
		tail -n 50 ../sof.git/dump-$j.txt
		exit 2;
	fi
done

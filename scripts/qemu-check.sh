#!/bin/bash

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl icl skl kbl cnl)
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

echo $PLATFORMS

for j in ${PLATFORMS[@]}
do
	FWNAME="sof-$j.ri"
	PLATFORM=$j
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
		SHM_IPC_REG=qemu-bridge-shim-io
		SHM_MBOX=qemu-bridge-mbox-io
	fi
	if [ $j == "hsw" ]
	then
		READY_IPC="00 fc 00 80"
		SHM_IPC_REG=qemu-bridge-shim-io
		SHM_MBOX=qemu-bridge-mbox-io
	fi
	if [ $j == "apl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG=qemu-bridge-ipc-io
		OUTBOX_OFFSET="f000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_$j/src/arch/xtensa/rom_$j.bin"
	fi
	if [ $j == "skl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG=qemu-bridge-ipc-io
		OUTBOX_OFFSET="f000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_$j/src/arch/xtensa/rom_$j.bin"
	fi
	if [ $j == "kbl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG=qemu-bridge-ipc-io
		OUTBOX_OFFSET="f000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_$j/src/arch/xtensa/rom_$j.bin"
	fi
	if [ $j == "cnl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG=qemu-bridge-ipc-io
		OUTBOX_OFFSET="5000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_$j/src/arch/xtensa/rom_$j.bin"
	fi
	if [ $j == "icl" ]
	then
		READY_IPC="00 00 00 f0"
		SHM_IPC_REG=qemu-bridge-ipc-io
		OUTBOX_OFFSET="5000"
		SHM_MBOX=qemu-bridge-hp-sram-mem
		ROM="-r ../sof.git/build_$j/src/arch/xtensa/rom_$j.bin"
	fi

	./xtensa-host.sh $PLATFORM -k ../sof.git/build_$j/src/arch/xtensa/sof_$j.ri $ROM -o 2.0 ../sof.git/dump-$j.txt
	# dump log into sof.git incase running in docker

	# check if ready ipc header is in the ipc regs
	IPC_REG=$(hexdump -C /dev/shm/$SHM_IPC_REG | grep "$READY_IPC")
	# check if ready ipc message is in the mbox
	IPC_MSG=$(hexdump -C /dev/shm/$SHM_MBOX | grep -A 4 "$READY_MSG" | grep -A 4 "$OUTBOX_OFFSET")

	if [ "$IPC_REG" ]; then
		echo "ipc reg dump:"
		# directly output the log to be nice to look
		hexdump -C /dev/shm/$SHM_IPC_REG | grep "$READY_IPC"
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

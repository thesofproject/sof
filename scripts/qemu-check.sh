#!/bin/bash

#SUPPORTED_PLATFORMS=(byt cht bdw hsw apl icl skl kbl cnl)
# TODO: skl, kbl, cnl is unstable need to check
SUPPORTED_PLATFORMS=(byt cht bdw hsw apl icl)

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
		READY_IPC="0x70028800"
	fi
	if [ $j == "cht" ]
	then
		READY_IPC="0x70028800"
	fi
	if [ $j == "bdw" ]
	then
		READY_IPC="0x70000000"
		fi
	if [ $j == "hsw" ]
	then
		READY_IPC="0x70000000"
	fi
	if [ $j == "apl" ]
	then
		PLATFORM="bxt"
		READY_IPC="0xf0000000"
		ROM="-r ../sof.git/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "skl" ]
	then
		READY_IPC="0xf0000000"
		ROM="-r ../sof.git/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "kbl" ]
	then
		READY_IPC="0xf0000000"
		ROM="-r ../sof.git/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "cnl" ]
	then
		READY_IPC="0xf0000000"
		ROM="-r ../sof.git/src/arch/xtensa/rom-$j.bin"
	fi
	if [ $j == "icl" ]
	then
		READY_IPC="0xf0000000"
		ROM="-r ../sof.git/src/arch/xtensa/rom-$j.bin"
	fi

	./xtensa-host.sh $PLATFORM -k ../sof.git/src/arch/xtensa/sof-$j.ri $ROM -o 2.0 ../sof.git/dump-$j.txt
	# dump log into sof.git incase running in docker

	# now check log for boot message
	if grep $READY_IPC ../sof.git/dump-$j.txt; then
		echo "Boot success";
	else
		echo "Error boot failed"
		tail -n 50 ../sof.git/dump-$j.txt
		exit 2;
	fi
done

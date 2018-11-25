#!/bin/bash

COUNTER=0
MAXLOOPS=100
touch test.log

# remove audio modules
remove()
{
	rmmod sof_acpi_dev
	rmmod snd_soc_acpi_intel_match
#	rmmod snd_sof_nocodec
#	rmmod snd_sof_intel_hsw
#	rmmod snd_sof_intel_bdw
	rmmod snd_sof_intel_byt
	rmmod snd_sof
	rmmod snd_soc_sst_bytcr_rt5651
	rmmod snd_soc_acpi
}

# reload audio modules
reload()
{
	modprobe sof_acpi_dev
}

err_check()
{
	unset FW_BOOT
	unset ERROR
	FW_BOOT=$(dmesg | grep sof-audio | grep "boot complete")
	ERROR=$(dmesg | grep sof-audio | grep "error")
	if [ ! -z "$ERROR" ] || [ -z "$FW_BOOT" ]
	then
        	echo "modules reload failed with error"
		dmesg > dmesg.log
		exit 1
	else
        	echo "audio modules are reloaded successfully"
	fi
}

playback()
{
	aplay -D hw:0,0 -r 48000 -c2 -f dat -d5 /dev/zero
}

main()
{
echo "modules reloadable test start: " > result.log
while [ $COUNTER -lt $MAXLOOPS ]; do
	echo "test $COUNTER"
	echo "audio modules are removing..."
	remove
	if [ $? -eq 0 ]; then
		echo "$COUNTER: modules remove - PASS" >> result.log
	else
		echo "$COUNTER: modules remove - FAIL" >> result.log
		dmesg > dmesg.log
		exit 1
	fi
	sleep 1

	# check modules is removed
	aplay -l |grep "device" > /dev/null
	if [ $? -ne 0 ]; then
        	echo "$COUNTER: check audio device after modules removed - PASS" >> result.log
	else
        	echo "$COUNTER: check audio device after modules removed - FAIL" >> result.log
		dmesg > dmesg.log
		exit 1
	fi
	sleep 1

	# reload the audio moduels
	echo "audio modules are reloading..."
	reload
	if [ $? -eq 0 ]; then
        	echo "$COUNTER: modules reload - PASS" >> result.log
	else
        	echo "$COUNTER: modules reload - FAIL" >> result.log
		dmesg > dmesg.log
		exit 1
	fi

	# check the error log
	err_check

	sleep 8 # wait for modules initComplete

	# check playback after modules reloaded
	playback
	if [ $? -eq 0 ]; then
        	echo "$COUNTER: playback after modules reloaded - PASS" >> result.log
	else
        	echo "$COUNTER: playback after modules reloaded - FAIL" >> result.log
		dmesg > dmesg.log
		exit 1
	fi

	sleep 1
	let COUNTER+=1
done
}

main

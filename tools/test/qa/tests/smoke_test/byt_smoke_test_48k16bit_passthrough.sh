#!/bin/bash

# default devices
psth_p="0,0"  # device ID of pass-through playback
psth_c="0,0"  # device ID of pass-through capture

# flag for modules status
flag=0

# audio modules list
modules_list="sof_acpi_dev snd_sof_intel_hsw snd_sof_intel_bdw snd_sof_intel_byt snd_soc_sst_bytcr_rt5651
			  snd_soc_rt5651 snd_sof snd_soc-core snd_soc_acpi snd_soc_acpi_intel_match snd_soc_rl6231"

# features passes vs. features all
feature_pass=0
feature_cnt=1

init_counter () {
	feature_pass=0
	feature_all=0
}

evaluate_result () {
	feature_cnt=$((feature_cnt+1))
	if [ $1 -eq 0 ]; then
		feature_pass=$((feature_pass+1))
		echo "pass"
		sleep 1
	else
		echo "fail"
		sleep 1
	fi
}

print_result () {
	let feature_cnt=$feature_cnt-1
	echo "[$feature_pass/$feature_cnt] features passes."
}

# test items
run_feature_test() {
	init_counter

	# check FW loading
	echo "$feature_cnt: check FW loading"
	dmesg |grep "firmware boot complete" > /dev/null
	evaluate_result $?
	# check sound modules
	echo "$feature_cnt: check sound modules"
	lsmod |grep "snd" > /dev/null
	evaluate_result $?
	# check sof modules
        echo "$feature_cnt: check sof modules"
	lsmod |grep "sof" > /dev/null
	evaluate_result $?
	# check PCMs
	echo "$feature_cnt: check playback PCMs list"
	aplay -l |grep "device"
	evaluate_result $?
	echo "$feature_cnt: check capture PCM list"
	arecord -l |grep "device" > /dev/null
	evaluate_result $?
	# check audio playback
	echo "$feature_cnt: check Mono wav playback with pass-through pipeline"
	aplay -Dhw:$psth_p -c1 -r48000 -f dat -d5 /dev/zero > /dev/null
	evaluate_result $?
	echo "$feature_cnt: check stereo wav playback with pass-through pipeline"
	aplay -Dhw:$psth_p -c2 -r48000 -f dat -d5 /dev/zero > /dev/null
	evaluate_result $?
	# check audio capture
	echo "$feature_cnt: check mono wav capture"
	arecord -Dhw:$path_c -c1 -r48000 -f dat -d2 /dev/zero
	evaluate_result $?
	echo "$feature_cnt: check stereo wav capture"
	arecord -Dhw:$psth_c -c2 -r48000 -f dat -d5 /dev/null
	evaluate_result $?
	# check modules unloadable
	echo "$feature_cnt: check the modules unloadable"
	for modules in $modules_list
        do
		rmmod $modules
        done
	lsmod |grep snd_soc
	if [ $? -eq 0 ]; then
		evaluate_result 1
		flag=0
	else
		evaluate_result 0
		flag=1
	fi
	# check the modules reloadable
	echo "$feature_cnt: check the modules reloadable"
	if [ $flag -eq 1 ]; then
		modprobe sof-acpi-dev
		if [ $? -eq 0 ]; then
			evaluate_result 0
			flag=0
	else
			evaluate_result 1
			flag=1
		fi
	else
        echo "modules unload failed, skip modules reloadable test"
		let feature_cnt=$feature_cnt-1
    fi
	# check playback/capture after modules reloaded
	sleep 8 # waiting for PCM device ready
	if [ $flag -eq 0 ]; then
		echo "$feature_cnt: check audio playback after modules reloaded"
		aplay -Dhw:$psth_p -c2 -r48000 -f dat -d5 /dev/zero
		evaluate_result $?
		echo "$feature_cnt: check audio capture after modules reloaded"
		arecord -Dhw:$psth_c -c2 -r48000 -f dat -d5 /dev/null
		evaluate_result $?
	else
		echo "modules reloading failed, skip the playback/capture check"
	fi

	print_result
}

echo "*******************************************"
echo "            SOF Smoke Test                 "
echo "-------------------------------------------"

# get device
echo "usage:"
echo "  $0 <device1-playback> <device1-playback> <device-capture>"

if [ $# -eq 2 ]; then
        psth_p=$1
	psth_c=$2
elif [ $# -eq 1 ]; then
        psth_p=$1
	psth_c=$1
fi

echo "current setting:"
echo " playback device: pass-through playback device: $psth_p \
	capture device: pass-through capture device: $psth_c"

# run test
run_feature_test
echo "*******************************************"

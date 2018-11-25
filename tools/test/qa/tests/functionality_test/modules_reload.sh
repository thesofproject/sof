#!/bin/bash

PLATFORM=$1

lsmod |egrep "snd|sof" > lsmod.list

SOF_MODULE=(sof_pci_dev sof_acpi_dev snd_soc_acpi_intel_match snd_sof_intel_byt snd_sof_intel_hsw snd_sof_intel_bdw snd_sof_intel_hda_common snd_sof_xtensa_dsp snd_soc_sst_bytcr_rt5640 snd_soc_sst_bytcr_rt5651 snd_soc_sst_cht_bsw_rt5645 snd_soc_sst_cht_bsw_rt5670 snd_soc_sst_byt_cht_da7213 snd_soc_sst_bxt_pcm512x snd_soc_sst_bxt_tdf8532 snd_soc_cnl_rt274 snd_sof_nocodec snd_sof snd_soc_rt5670 snd_soc_rt5645 snd_soc_rt5651 snd_soc_rt5640 snd_soc_rl6231 snd_soc_da7213 snd_soc_pcm512x_i2c snd_soc_pcm512x snd_soc_tdf8532 snd_soc_rt274 snd_soc_acpi)
RELOAD_MODULE=(modprobe snd_soc_rt5670 modprobe snd_soc_rt5645 modprobe snd_soc_rt5651 modprobe snd_soc_rt5640 modprobe snd_soc_da7213 modprobe snd_soc_pcm512x_i2c modprobe snd_soc_tdf8532 modprobe snd_soc_rt274 modprobe sof_acpi_dev modprobe sof_pci_dev)

LOAD_MODULES=($(awk '{print $1}' lsmod.list))

ERR_COUNT_BEFORE=`dmesg | grep sof-audio | grep "error" | wc -l`

# unload audio modules
remove()
{
	for module in ${SOF_MODULE[@]}
	do
		if lsmod | grep "$module" &> /dev/null ; then
			echo "Removing $module"
			rmmod $module
		else
			echo "skipping $module, not loaded"
		fi
	done
}

err_check()
{
	ERR_COUNT_AFTER=`dmesg | grep sof-audio | grep "error" | wc -l`
	if [ $ERR_COUNT_AFTER -gt $ERR_COUNT_BEFORE ]
	then
		exit 1
	else
		exit 0
	fi
}

# reload audio modules
insert(){
RELOAD_MODULE=(snd_soc_rt5670 snd_soc_rt5645 snd_soc_rt5651 snd_soc_rt5640 snd_soc_da7213 snd_soc_pcm512x_i2c snd_soc_tdf8532 sof_acpi_dev sof_pci_dev)
	for module in ${RELOAD_MODULE[@]}
	do
		modprobe $module
	done
}

remove
sleep 2
insert
err_check

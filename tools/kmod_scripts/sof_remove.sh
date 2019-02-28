#!/bin/bash

remove_module() {

    MODULE="$1"

    if lsmod | grep "$MODULE" &> /dev/null ; then
	echo "Removing $MODULE"
	rmmod $MODULE
    else
	echo "skipping $MODULE, not loaded"
    fi
}

remove_module sof_pci_dev
remove_module sof_acpi_dev
remove_module snd_sof_intel_byt
remove_module snd_sof_intel_hsw
remove_module snd_sof_intel_bdw
remove_module snd_sof_intel_hda_common
remove_module snd_sof_intel_hda
remove_module snd_sof_xtensa_dsp
remove_module snd_soc_acpi_intel_match

remove_module snd_soc_sst_bytcr_rt5640
remove_module snd_soc_sst_cht_bsw_rt5645
remove_module snd_soc_sst_bytcr_rt5651
remove_module snd_soc_sst_cht_bsw_rt5670
remove_module snd_soc_sof_rt5682
remove_module snd_soc_cnl_rt274
remove_module snd_soc_sst_byt_cht_da7213
remove_module snd_soc_sst_bxt_pcm512x
remove_module snd_soc_sst_bxt_wm8804
remove_module snd_soc_skl_hda_dsp
remove_module snd_soc_sst_bxt_da7219_max98357a

remove_module snd_sof
remove_module snd_sof_nocodec

remove_module snd_soc_rt5640
remove_module snd_soc_rt5645
remove_module snd_soc_rt5651
remove_module snd_soc_rt5670
remove_module snd_soc_rt5682
remove_module snd_soc_rl6231
remove_module snd_soc_rt274
remove_module snd_soc_da7213
remove_module snd_soc_da7219
remove_module snd_soc_pcm512x_i2c
remove_module snd_soc_pcm512x
remove_module snd_soc_wm8804_i2c
remove_module snd_soc_wm8804
remove_module snd_soc_hdac_hda
remove_module snd_soc_hdac_hdmi
remove_module snd_soc_dmic
remove_module snd_soc_max98357a

remove_module snd_soc_acpi
remove_module snd_hda_ext_core

remove_module snd_soc_core
remove_module snd_hda_codec
remove_module snd_hda_core
remove_module snd_hwdep
remove_module snd_pcm

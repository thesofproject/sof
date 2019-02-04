#!/bin/bash

insert_module() {

    MODULE="$1"

    if modinfo "$MODULE" &> /dev/null ; then
	modprobe $MODULE
    else
	echo "skipping $MODULE, not in tree"
    fi
}

insert_module snd_soc_rt5640
insert_module snd_soc_rt5645
insert_module snd_soc_rt5651
insert_module snd_soc_rt5670
insert_module snd_soc_rt5682
insert_module snd_soc_rt274
insert_module snd_soc_da7213
insert_module snd_soc_pcm512x_i2c
insert_module snd_soc_wm8804_i2c

insert_module sof_acpi_dev
insert_module sof_pci_dev



#!/bin/bash

modprobe snd_soc_rt5670
modprobe snd_soc_rt5645
modprobe snd_soc_rt5651
modprobe snd_soc_rt5640
modprobe snd_soc_da7213
modprobe snd_soc_pcm512x_i2c

modprobe sof_acpi_dev
modprobe sof_pci_dev



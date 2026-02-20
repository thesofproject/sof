#!/bin/sh

selected_tplg=$1
target_tplg="/lib/firmware/imx/sof-tplg/sof-imx8-wm8960.tplg"

cp "$target_tplg" /lib/firmware/imx/sof-tplg/previous-topology.tplg

rmmod snd_sof_imx8
rmmod snd_sof_xtensa_dsp
rmmod snd_sof_of
rmmod imx_common
rmmod snd_sof
rmmod snd_sof_utils
rmmod snd_soc_wm8960
rmmod snd_soc_simple_card

cp "$selected_tplg" "$target_tplg"

modprobe snd_soc_simple_card
modprobe snd_soc_wm8960
modprobe snd_sof_utils
modprobe snd_sof
modprobe imx_common
modprobe snd_sof_of
modprobe snd_sof_xtensa_dsp
modprobe snd_sof_imx8

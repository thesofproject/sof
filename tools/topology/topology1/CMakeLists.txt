# SPDX-License-Identifier: BSD-3-Clause

file(GLOB TPLG_DEPS
	platform/intel/*.m4
	platform/common/*.m4
	common/*.m4
	m4/*.m4
	sof/*.m4
)

add_custom_target(abi_v1
	DEPENDS abi.h
)
add_custom_command(OUTPUT abi.h
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
	DEPENDS ${TPLG_DEPS} ${SOF_ROOT_SOURCE_DIRECTORY}/src/include/kernel/abi.h
	# get_abi.h generates (a smaller) abi.h in the current directory
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	VERBATIM
	USES_TERMINAL
)

set(TPLGS
	## HDaudio codec topologies
	"sof-hda-generic\;sof-hda-generic\;-DCHANNELS=0\;-DHSPROC=volume\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-1ch\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-1ch-pdm1\;-DPDM1\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-2ch\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-2ch-pdm1\;-DPDM1\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-3ch\;-DCHANNELS=4\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-4ch\;-DCHANNELS=4\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	## end HDaudio codec topologies

	"sof-hda-generic-idisp\;sof-hda-generic-idisp\;-DCHANNELS=0\;-DDYNAMIC=1"
	"sof-hda-generic-idisp\;sof-hda-generic-idisp-2ch\;-DCHANNELS=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic-idisp\;sof-hda-generic-idisp-4ch\;-DCHANNELS=4\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-tgl-nocodec\;-DPLATFORM=tgl\;-DDMIC_48k_CORE_ID=1\;-DSSP0_CORE_ID=2\;-DSSP1_CORE_ID=3\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-tgl-h-nocodec\;-DPLATFORM=tgl\;-DNCORES=2\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-ehl-nocodec\;-DPLATFORM=ehl\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-adl-nocodec\;-DPLATFORM=adl\;-DDMIC_48k_CORE_ID=1\;-DSSP0_CORE_ID=2\;-DSSP1_CORE_ID=3\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-rpl-nocodec\;-DPLATFORM=rpl\;-DDMIC_48k_CORE_ID=1\;-DSSP0_CORE_ID=2\;-DSSP1_CORE_ID=3\;-DDYNAMIC=1"

	## SoundWire topologies
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1308-mono-rt715\;-DPLATFORM=tgl\;-DMONO\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-l0-rt1316-l1-mono-rt714-l3\;-DPLATFORM=tgl\;-DMONO\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt715-rt711-rt1308-mono\;-DPLATFORM=tgl\;-DMONO\;-DUAJ_LINK=1\;-DAMP_1_LINK=2\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1308-rt715\;-DPLATFORM=tgl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-tgl-rt711-rt1316-rt714\;-DPLATFORM=tgl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l0-rt1308-l12-rt715-l3\;-DPLATFORM=adl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l0-rt1316-l12-rt714-l3\;-DPLATFORM=adl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l0-rt1316-l13-rt714-l2\;-DPLATFORM=adl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=3\;-DEXT_AMP_REF\;-DMIC_LINK=2"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l2-rt1316-l01-rt714-l3\;-DPLATFORM=adl\;-DUAJ_LINK=2\;-DAMP_1_LINK=0\;-DAMP_2_LINK=1\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt711-l2-rt1316-l01\;-DPLATFORM=adl\;-DUAJ_LINK=2\;-DAMP_1_LINK=0\;-DAMP_2_LINK=1\;-DEXT_AMP_REF\;-DNO_LOCAL_MIC"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l2\;-DPLATFORM=rpl\;-DUAJ_LINK=2\;-DNOAMP\;-DNO_LOCAL_MIC"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l0\;-DPLATFORM=rpl\;-DUAJ_LINK=0\;-DNOAMP\;-DNO_LOCAL_MIC"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l2-rt1316-l01-rt714-l3\;-DPLATFORM=rpl\;-DUAJ_LINK=2\;-DAMP_1_LINK=0\;-DAMP_2_LINK=1\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l2-rt1316-l01\;-DPLATFORM=rpl\;-DUAJ_LINK=2\;-DAMP_1_LINK=0\;-DAMP_2_LINK=1\;-DEXT_AMP_REF\;-DNO_LOCAL_MIC"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l0-rt1316-l12-rt714-l3\;-DPLATFORM=rpl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l0-rt1318-l12-rt714-l3\;-DPLATFORM=rpl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt711-l0-rt1318-l12\;-DPLATFORM=rpl\;-DUAJ_LINK=0\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DNO_LOCAL_MIC"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-2ch\;-DCHANNELS=2\;-DEXT_AMP\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-4ch\;-DCHANNELS=4\;-DEXT_AMP\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-4ch\;-DCHANNELS=4\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-4ch\;-DCHANNELS=4\;-DBT_OFFLOAD\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711\;-DCHANNELS=0\;-DBT_OFFLOAD\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-l0-rt1316-l3-2ch\;-DCHANNELS=2\;-DEXT_AMP\;-DEXT_AMP_REF\;-DAMP_1_LINK=3\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-l0-rt1316-l2-2ch\;-DCHANNELS=2\;-DEXT_AMP\;-DEXT_AMP_REF\;-DAMP_1_LINK=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-l0-rt1316-l2-4ch\;-DCHANNELS=4\;-DEXT_AMP\;-DEXT_AMP_REF\;-DAMP_1_LINK=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-rpl-rt711-4ch\;-DCHANNELS=4\;-DBT_OFFLOAD\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=rpl"
	"sof-tgl-rt711-rt1308\;sof-rpl-rt711\;-DCHANNELS=0\;-DBT_OFFLOAD\;-DPLATFORM=rpl"
	## end SoundWire topologies

	## SoundWire NOJACK topologies
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l1-mono-rt714-l0\;-DPLATFORM=adl\;-DMONO\;-DNOJACK\;-DAMP_1_LINK=1\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l2-mono-rt714-l0\;-DPLATFORM=adl\;-DMONO\;-DNOJACK\;-DAMP_1_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l12-rt714-l0\;-DPLATFORM=adl\;-DNOJACK\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l2-mono-rt714-l3\;-DPLATFORM=adl\;-DMONO\;-DNOJACK\;-DAMP_1_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-rpl-rt1316-l12-rt714-l0\;-DPLATFORM=rpl\;-DNOJACK\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	## end SoundWire NOJACK topologies

	"sof-ehl-rt5660\;sof-ehl-rt5660\;-DHDMI=1"
	"sof-ehl-rt5660\;sof-ehl-rt5660-nohdmi"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98357a-rt5682\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=2"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98357a-rt5682-rtnr\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=2\;-DCHANNELS=2\;-DRTNR"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98357a-rt5682-waves\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=2\;-DWAVES"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98357a-rt5682-2way\;-DCODEC=MAX98360A_TDM\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=2\;-D2CH_2WAY"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98357a-rt5682-waves-2way\;-DCODEC=MAX98360A_TDM\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=2\;-D2CH_2WAY\;-DWAVES"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98357a-rt5682-4ch\;-DCODEC=MAX98360A_TDM\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=2\;-D4CH_PASSTHROUGH"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682\;-DCODEC=MAX98360A\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DSPK_MIC_PERIOD_US=10000"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-waves\;-DCODEC=MAX98360A\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DWAVES"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-2way\;-DCODEC=MAX98360A_TDM\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-D2CH_2WAY\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-2way-pdm1\;-DCODEC=MAX98360A_TDM\;-DFMT=s32le\;-DDMIC_DAI_LINK_16k_PDM=STEREO_PDM1\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-D2CH_2WAY\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-4ch\;-DCODEC=MAX98360A_TDM\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-D4CH_PASSTHROUGH\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-rtnr\;-DCODEC=MAX98360A\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DCHANNELS=2\;-DRTNR"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-rtnr-google-aec\;-DCODEC=MAX98360A\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DCHANNELS=2\;-DGOOGLE_RTC_AUDIO_PROCESSING\;-DRTNR\;-DNOHOTWORD\;-DNO16KDMIC"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682-pdm1\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DDMIC_DAI_LINK_16k_PDM=STEREO_PDM1\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682-pdm1-google-aec\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DDMIC_DAI_LINK_16k_PDM=STEREO_PDM1\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DCHANNELS=2\;-DGOOGLE_RTC_AUDIO_PROCESSING"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682-pdm1-drceq\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DDMIC_DAI_LINK_16k_PDM=STEREO_PDM1\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DDRC_EQ"
	"sof-tgl-max98357a-rt5682\;sof-tgl-rt1011-rt5682\;-DCODEC=RT1011\;-DFMT=s24le\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682-rtnr\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DCHANNELS=2\;-DRTNR"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-rt5682\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-rt5682-rtnr\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DCHANNELS=2\;-DRTNR\;-DNOHOTWORD\;-DDMICPROC=rtnr\;-DNO16KDMIC\;-DDMIC_48k_CORE_ID=1"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-rt5682-google-aec\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DCHANNELS=2\;-DGOOGLE_RTC_AUDIO_PROCESSING\;-DNOHOTWORD\;-DNO16KDMIC"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-rt5682-rtnr-google-aec\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DCHANNELS=2\;-DGOOGLE_RTC_AUDIO_PROCESSING\;-DRTNR\;-DNOHOTWORD\;-DNO16KDMIC\;-DSPK_MIC_PERIOD_US=10000"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-ssp2-rt5682-ssp0\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=2"
	"sof-tgl-max98357a-rt5682\;sof-adl-rt5682\;-DNO_AMP\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682"
	"sof-tgl-max98357a-rt5682\;sof-adl-rt1019-rt5682\;-DCODEC=RT1019\;-DFMT=s24le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DSPK_MIC_PERIOD_US=10000\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-rt1019-rt5682-waves\;-DCODEC=RT1019\;-DFMT=s24le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DWAVES\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-cs35l41\;-DCODEC=CS35L41\;-DFMT=s24le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_ssp_amp\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DNO_HEADPHONE"
	"sof-tgl-max98357a-rt5682\;sof-adl-cs35l41-waves\;-DCODEC=CS35L41\;-DFMT=s24le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_ssp_amp\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DNO_HEADPHONE\;-DWAVES\;-DNOHOTWORD\;-DNO16KDMIC"
	"sof-tgl-max98357a-rt5682\;sof-adl-cs35l41-google-aec-waves\;-DCODEC=CS35L41\;-DFMT=s24le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_ssp_amp\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DNO_HEADPHONE\;-DGOOGLE_RTC_AUDIO_PROCESSING\;-DWAVES\;-DNOHOTWORD\;-DNO16KDMIC\;-DSPK_MIC_PERIOD_US=10000"
	"sof-tgl-max98373-rt5682\;sof-tgl-max98373-rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98373-rt5682\;sof-tgl-max98373-rt5682-igonr\;-DAMP_SSP=1\;-DIGO"
	"sof-tgl-max98373-rt5682\;sof-tgl-max98373-rt5682-xperi\;-DAMP_SSP=1\;-DINCLUDE_IIR_EQ=1"
	"sof-tgl-max98373-rt5682\;sof-tgl-rt5682-ssp0-max98373-ssp2\;-DAMP_SSP=2"
	"sof-tgl-max98373-rt5682\;sof-tgl-rt5682-ssp0-max98373-ssp2-xperi\;-DAMP_SSP=2\;-DINCLUDE_IIR_EQ=1"
	"sof-adl-nau8825\;sof-adl-nau8825\;-DNO_AMP\;-DBT_OFFLOAD"
	"sof-adl-nau8825\;sof-adl-max98373-nau8825\;-DAMP_SSP=1\;-DSMART_AMP\;-DBT_OFFLOAD\;-DNOHOTWORD\;-DNO16KDMIC"
	"sof-adl-nau8825\;sof-adl-max98373-nau8825-dts\;-DAMP_SSP=1\;-DSMART_AMP\;-DBT_OFFLOAD\;-DNOHOTWORD\;-DNO16KDMIC\;-DDTS=`DTS'"
	"sof-adl-nau8825\;sof-adl-max98360a-nau8825\;-DCODEC=MAX98360A\;-DFMT=s16le\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DSPK_MIC_PERIOD_US=10000"
	"sof-adl-nau8825\;sof-adl-rt1019-nau8825\;-DCODEC=RT1019P\;-DFMT=s16le\;-DAMP_SSP=2\;-DSPK_MIC_PERIOD_US=10000"
	"sof-adl-nau8825\;sof-adl-rt1015-nau8825\;-DCODEC=RT1015P\;-DFMT=s32le\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DSPK_MIC_PERIOD_US=10000"
	"sof-adl-nau8825\;sof-adl-nau8318-nau8825\;-DCODEC=NAU8318\;-DFMT=s16le\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DSPK_MIC_PERIOD_US=10000"
	"sof-tgl-sdw-max98373-rt5682\;sof-tgl-sdw-max98373-rt5682\;-DCHANNELS=4\;-DPLATFORM=tgl"
	"sof-tgl-sdw-max98373-rt5682\;sof-adl-sdw-max98373-rt5682\;-DCHANNELS=4\;-DPLATFORM=adl"
	"sof-smart-amplifier-nocodec\;sof-smart-amplifier-nocodec"

	"sof-glk-es8336\;sof-tgl-es8336\;-DPLATFORM=tgl\;-DSSP_NUM=0\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-tgl-es8336-ssp0\;-DPLATFORM=tgl\;-DSSP_NUM=0\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-tgl-es8336-ssp1\;-DPLATFORM=tgl\;-DSSP_NUM=1\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-tgl-es8336-ssp2\;-DPLATFORM=tgl\;-DSSP_NUM=2\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-tgl-es8336-dmic2ch-ssp0\;-DPLATFORM=tgl\;-DSSP_NUM=0\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-tgl-es8336-dmic2ch-ssp1\;-DPLATFORM=tgl\;-DSSP_NUM=1\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-tgl-es8336-dmic2ch-ssp2\;-DPLATFORM=tgl\;-DSSP_NUM=2\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-tgl-es8336-dmic4ch-ssp0\;-DPLATFORM=tgl\;-DSSP_NUM=0\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-tgl-es8336-dmic4ch-ssp1\;-DPLATFORM=tgl\;-DSSP_NUM=1\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-tgl-es8336-dmic4ch-ssp2\;-DPLATFORM=tgl\;-DSSP_NUM=2\;-DCHANNELS=4"

	"sof-glk-es8336\;sof-adl-es8336\;-DPLATFORM=adl\;-DSSP_NUM=0\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-adl-es8336-ssp0\;-DPLATFORM=adl\;-DSSP_NUM=0\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-adl-es8336-ssp1\;-DPLATFORM=adl\;-DSSP_NUM=1\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-adl-es8336-ssp2\;-DPLATFORM=adl\;-DSSP_NUM=2\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-adl-es8336-dmic2ch-ssp0\;-DPLATFORM=adl\;-DSSP_NUM=0\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-adl-es8336-dmic2ch-ssp1\;-DPLATFORM=adl\;-DSSP_NUM=1\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-adl-es8336-dmic2ch-ssp2\;-DPLATFORM=adl\;-DSSP_NUM=2\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-adl-es8336-dmic4ch-ssp0\;-DPLATFORM=adl\;-DSSP_NUM=0\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-adl-es8336-dmic4ch-ssp1\;-DPLATFORM=adl\;-DSSP_NUM=1\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-adl-es8336-dmic4ch-ssp2\;-DPLATFORM=adl\;-DSSP_NUM=2\;-DCHANNELS=4"

	#sof-adl-es8336-ssp1-hdmi-ssp02 supports es8336 codec along with 2xHDMI_over_SSP Capture's.
	"sof-glk-es8336\;sof-adl-es8336-ssp1-hdmi-ssp02\;-DPLATFORM=adl\;-DSSP_NUM=1\;-DCHANNELS=0\;-DHDMI_1_SSP_NUM=0\;-DHDMI_2_SSP_NUM=2"

	#To support HDMI-in capture with RT1308 ssp-amplifier products.
	"sof-tgl-rt1308-hdmi-ssp\;sof-tgl-rt1308-ssp2-hdmi-ssp15\;-DAMP_SSP_NUM=2\;-DHDMI_1_SSP_NUM=1\;-DHDMI_2_SSP_NUM=5\;-DPLATFORM=tgl"
	#Topologies for No SSP-amp or codec+HDMI-IN capture support.
	"sof-tgl-rt1308-hdmi-ssp\;sof-tgl-nocodec-hdmi-ssp15\;-DNO_AMP\;-DHDMI_1_SSP_NUM=1\;-DHDMI_2_SSP_NUM=5\;-DPLATFORM=tgl"
	"sof-tgl-rt1308-hdmi-ssp\;sof-adl-nocodec-hdmi-ssp02\;-DNO_AMP\;-DHDMI_1_SSP_NUM=0\;-DHDMI_2_SSP_NUM=2\;-DPLATFORM=adl"

	## i.MX8 (i.MX8QM and i.MX8QXP) topologies
	"sof-imx8-nocodec\;sof-imx8-nocodec"
	"sof-imx8-wm8960-cs42888\;sof-imx8-wm8960-cs42888"
	"sof-imx8-cs42888\;sof-imx8-cs42888"
	"sof-imx8-nocodec-sai\;sof-imx8-nocodec-sai"
	"sof-imx8-wm8960-mixer\;sof-imx8-wm8960-mixer\;-DCODEC=wm8960\;-DRATE=48000\;-DSAI_INDEX=1"
	"sof-imx8-wm8960-mixer\;sof-imx8-wm8962-mixer\;-DCODEC=wm8962\;-DRATE=48000\;-DSAI_INDEX=1"
	"sof-imx8-compr-wm8960-mixer\;sof-imx8-compr-wm8960-mixer"
	"sof-imx8-wm8960-kwd\;sof-imx8-wm8960-kwd"
	"sof-imx8-cs42888-mixer\;sof-imx8-cs42888-mixer"
	"sof-imx8-wm8960\;sof-imx8-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-eq-iir-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=eq-iir-volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-eq-iir-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=eq-iir-volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-eq-fir-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=eq-fir-volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-eq-fir-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=eq-fir-volume\;-DSAI_INDEX=1"
	"sof-imx8-wm8960\;sof-imx8-drc-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=drc\;-DSAI_INDEX=1"
	## end i.MX8 (i.MX8QM and i.MX8QXP) topologies

	## i.MX8MP topologies
	"sof-imx8mp-compr-wm8960-mixer\;sof-imx8mp-compr-wm8960-mixer"
	"sof-imx8-wm8960-mixer\;sof-imx8mp-wm8960-mixer\;-DCODEC=wm8960\;-DRATE=48000\;-DSAI_INDEX=3"
	"sof-imx8-wm8960-mixer\;sof-imx8mp-wm8962-mixer\;-DCODEC=wm8962\;-DRATE=48000\;-DSAI_INDEX=3"
	"sof-imx8mp-wm8960-kwd\;sof-imx8mp-wm8960-kwd"
	"sof-imx8-wm8960\;sof-imx8mp-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-wm8904\;-DCODEC=wm8904\;-DRATE=44100\;-DPPROC=volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-eq-iir-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=eq-iir-volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-eq-iir-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=eq-iir-volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-eq-fir-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=eq-fir-volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-eq-fir-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=eq-fir-volume\;-DSAI_INDEX=3"
	"sof-imx8-wm8960\;sof-imx8mp-drc-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=drc\;-DSAI_INDEX=3"
	## end i.MX8MP topologies

	## i.MX8ULP topologies
	"sof-imx8ulp-btsco\;sof-imx8ulp-btsco\;-DRATE=8000"
	"sof-imx8ulp-btsco\;sof-imx8ulp-btsco-16k\;-DRATE=16000"
	"sof-imx8ulp-9x9-btsco\;sof-imx8ulp-9x9-btsco\;-DRATE=8000"
	"sof-imx8ulp-9x9-btsco\;sof-imx8ulp-9x9-btsco-16k\;-DRATE=16000"
	## end i.MX8ULP topologies

	## i.MX93 topologies
	"sof-imx93-wm8962\;sof-imx93-wm8962\;-DPPROC=volume"
	## end i.MX93 topologies

	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-rt1019-rt5682"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-rt1019-rt5682-dts\;-DDTS=`DTS'"
        "sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682-rtnr\;-DCHANNELS=2\;-DRTNR"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682-google-aec-rtnr\;-DGOOGLE_RTC_AUDIO_PROCESSING\;-DCHANNELS=2\;-DRTNR"
	"sof-mt8186-mt6366\;sof-mt8186"
	"sof-mt8186-mt6366\;sof-mt8186-dts\;-DDTS=`DTS'"
	"sof-mt8186-mt6366\;sof-mt8186-mt6366-rt1019-rt5682s-waves\;-DWAVES=1"
	"sof-mt8188-mt6359\;sof-mt8188"

	"sof-acp-renoir\;sof-acp"
	"sof-rn-rt5682-rt1019\;sof-rn-rt5682-rt1019"
	"sof-rn-rt5682-rt1019\;sof-rn-rt5682-rt1019-dts\;-DDTS=`DTS'"
	"sof-rn-rt5682-max98360\;sof-rn-rt5682-max98360"
	"sof-acp-rmb\;sof-acp-rmb"
	"sof-acp-rmb-dmic4ch\;sof-acp-rmb-dmic4ch"
	"sof-acp-rmb-tdm8ch-dmic4ch-rt5682-rt1019\;sof-acp-rmb-tdm8ch-dmic4ch-rt5682-rt1019"
	"sof-acp-rmb-tdm8ch-dmic2ch-nau8825-max98360\;sof-acp-rmb-tdm8ch-dmic2ch-nau8825-max98360"
	"sof-acp-rmb\;sof-rmb-rt5682s-rt1019"
	"sof-acp-rmb\;sof-rmb-nau8825-max98360"
)

# This empty 'production/' source subdirectory exists only to create the
# corresponding 'production/' subdirectory in the build directory and
# solve race https://github.com/thesofproject/sof/issues/5067 without
# the disruption of actually moving all *.m4 sources one directory level
# down.
add_subdirectory(production)

# "Install" all v1 production topologies two directory levels up.
add_custom_target(topologies1 ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory production ${SOF_TOPOLOGY_BINARY_DIRECTORY}
)

foreach(tplg ${TPLGS})
	list(GET tplg 0 input)
	list(GET tplg 1 output)
	list(LENGTH tplg tplg_len)
	math(EXPR num_args "${tplg_len} - 1")
	set(DEFINES "")
	if (${num_args} GREATER 1)
		foreach(index RANGE 2 ${num_args})
			list(GET tplg ${index} custom_define)
			list(APPEND DEFINES ${custom_define})
		endforeach()
	endif()

	# m4
	# TODO: reduce duplication with subdirectories
	add_custom_command(
		OUTPUT production/${output}.conf
		COMMAND m4 --fatal-warnings
			${DEFINES}
			-I ${CMAKE_CURRENT_SOURCE_DIR}/m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/platform/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}
			-I ${CMAKE_CURRENT_BINARY_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}/common/abi.m4
			${CMAKE_CURRENT_SOURCE_DIR}/${input}.m4
			> production/${output}.conf
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${input}.m4
		# The 'abi.h' file is the real dependency that triggers
		# a rebuild. 'abi_v1' is the target that avoids a race
		# to rebuild abi.h. See the CMake FAQ or (better) Sam
		# Thursfield's blog about custom targets.
		DEPENDS abi_v1 ${CMAKE_CURRENT_BINARY_DIR}/abi.h
		VERBATIM
		USES_TERMINAL
	)

	# alsatplg
	add_alsatplg_command(production/${output}.conf production/${output}.tplg)

	add_custom_target(topology1_${output} DEPENDS production/${output}.tplg)
	add_dependencies(topologies1 topology1_${output})
endforeach()

add_subdirectory(development)
add_subdirectory(dsp_enhancement)
add_subdirectory(kernel_dependent)

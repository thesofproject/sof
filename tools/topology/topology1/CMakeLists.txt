# SPDX-License-Identifier: BSD-3-Clause

file(GLOB TPLG_DEPS
	platform/intel/*.m4
	platform/common/*.m4
	common/*.m4
	m4/*.m4
	sof/*.m4
)

add_custom_target(abi_v1
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
	DEPENDS ${TPLG_DEPS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	VERBATIM
	USES_TERMINAL
)

set(TPLGS
	"sof-cht-nocodec\;sof-cht-nocodec\;-DPLATFORM=cht-nocodec\;-DSSP_NUM=2"
	"sof-cht-nocodec\;sof-byt-nocodec\;-DPLATFORM=byt-nocodec\;-DSSP_NUM=2"
	"sof-bdw-nocodec\;sof-bdw-nocodec"

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
	"sof-apl-keyword-detect\;sof-apl-keyword-detect"
	"sof-bdw-codec\;sof-bdw-rt286\;-DCODEC=RT286"
	"sof-bdw-codec\;sof-bdw-rt5640\;-DCODEC=RT5640"
	"sof-bdw-codec\;sof-bdw-rt5677\;-DCODEC=RT5677"
	"sof-byt-codec\;sof-byt-rt5640\;-DCODEC=RT5640\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5645\;-DCODEC=RT5645\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5651\;-DCODEC=RT5651\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5670\;-DCODEC=RT5670\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5682\;-DCODEC=RT5682\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-da7213\;-DCODEC=DA7213\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-cx2072x\;-DCODEC=CX2072X\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-es8316\;-DCODEC=ES8316\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-byt-rt5640-ssp0\;-DCODEC=RT5640\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-rt5645-ssp0\;-DCODEC=RT5645\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-rt5651-ssp0\;-DCODEC=RT5651\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-rt5670-ssp0\;-DCODEC=RT5670\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-rt5682-ssp0\;-DCODEC=RT5682\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-da7213-ssp0\;-DCODEC=DA7213\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-cx2072x-ssp0\;-DCODEC=CX2072X\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-es8316-ssp0\;-DCODEC=ES8316\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-byt-wm5102-ssp0\;-DCODEC=WM5102\;-DPLATFORM=byt-codec\;-DSSP_NUM=0"
	"sof-byt-codec\;sof-cht-rt5640\;-DCODEC=RT5640\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-rt5645\;-DCODEC=RT5645\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-rt5651\;-DCODEC=RT5651\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-rt5670\;-DCODEC=RT5670\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-rt5682\;-DCODEC=RT5682\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-da7213\;-DCODEC=DA7213\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-cx2072x\;-DCODEC=CX2072X\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-es8316\;-DCODEC=ES8316\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-byt-codec\;sof-cht-nau8824\;-DCODEC=NAU8824\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-cht-max98090\;sof-byt-max98090\;-DCODEC=MAX98090\;-DPLATFORM=byt-codec\;-DSSP_NUM=2"
	"sof-cht-max98090\;sof-cht-max98090\;-DCODEC=MAX98090\;-DPLATFORM=cht-codec\;-DSSP_NUM=2"
	"sof-cnl-rt274\;sof-cnl-rt274"
	"sof-apl-tdf8532\;sof-apl-tdf8532"
	"sof-apl-pcm512x\;sof-apl-pcm512x\;-DFSYNC=48000"
	"sof-apl-pcm512x\;sof-apl-pcm512x-master\;-DCODEC_MASTER\;-DFSYNC=48000"
	"sof-apl-pcm512x\;sof-apl-pcm512x-master-44100\;-DCODEC_MASTER\;-DFSYNC=44100"
	"sof-apl-demux-pcm512x\;sof-apl-demux-pcm512x"
	"sof-apl-rt298\;sof-apl-rt298"
	"sof-apl-wm8804\;sof-apl-wm8804"
	"sof-apl-da7219\;sof-apl-da7219"
	"sof-glk-da7219-kwd\;sof-glk-da7219-kwd\;-DPLATFORM=glk"
	"sof-glk-da7219-kwd\;sof-cml-da7219-max98357a\;-DPLATFORM=cml"
	"sof-glk-da7219-kwd\;sof-cml-da7219-max98390\;-DCODEC=MAX98390\;-DPLATFORM=cml"
	"sof-glk-da7219\;sof-glk-da7219\;-DHEADPHONE=da7219"
	"sof-glk-da7219\;sof-glk-cs42l42\;-DHEADPHONE=cs42l42"
	"sof-glk-rt5682\;sof-glk-rt5682"
	"sof-cavs-nocodec\;sof-apl-nocodec\;-DPLATFORM=bxt\;-DDYNAMIC=1\;-DDISABLE_SSP0\;-DDISABLE_SSP1"
	"sof-cavs-nocodec\;sof-glk-nocodec\;-DPLATFORM=bxt\;-DDYNAMIC=1\;-DDISABLE_SSP0\;-DDISABLE_SSP1"
	"sof-cavs-nocodec\;sof-cnl-nocodec\;-DPLATFORM=cnl\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-cml-nocodec\;-DPLATFORM=cml\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-icl-nocodec\;-DPLATFORM=icl\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-jsl-nocodec\;-DPLATFORM=jsl\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-tgl-nocodec\;-DPLATFORM=tgl\;-DDMIC_48k_CORE_ID=1\;-DSSP0_CORE_ID=2\;-DSSP1_CORE_ID=3\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-tgl-h-nocodec\;-DPLATFORM=tgl\;-DNCORES=2\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-ehl-nocodec\;-DPLATFORM=ehl\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-adl-nocodec\;-DPLATFORM=adl\;-DDMIC_48k_CORE_ID=1\;-DSSP0_CORE_ID=2\;-DSSP1_CORE_ID=3\;-DDYNAMIC=1"
	"sof-cavs-nocodec\;sof-rpl-nocodec\;-DPLATFORM=rpl\;-DDMIC_48k_CORE_ID=1\;-DSSP0_CORE_ID=2\;-DSSP1_CORE_ID=3\;-DDYNAMIC=1"
	"sof-icl-dmic-4ch\;sof-icl-dmic-4ch"
	"sof-cml-rt5682\;sof-cml-rt5682\;-DPLATFORM=cml\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-whl-rt5682\;-DPLATFORM=whl\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-icl-rt5682\;-DPLATFORM=icl\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682-kwd\;sof-cml-rt5682-kwd\;-DPLATFORM=cml"
	"sof-cml-rt5682-kwd\;sof-whl-rt5682-kwd\;-DPLATFORM=whl"
	"sof-cml-rt5682-kwd\;sof-icl-rt5682-kwd\;-DPLATFORM=icl"
	"sof-cml-demux-rt5682\;sof-cml-demux-rt5682\;-DPLATFORM=cml"
	"sof-cml-demux-rt5682\;sof-whl-demux-rt5682\;-DPLATFORM=whl"
	"sof-cml-rt5682-max98357a\;sof-cml-rt5682-max98357a\;-DPLATFORM=cml"
	"sof-cml-demux-rt5682-max98357a\;sof-cml-demux-rt5682-max98357a\;-DPLATFORM=cml"
	"sof-cml-rt1011-rt5682\;sof-cml-rt1011-rt5682\;-DPLATFORM=cml\;-DPPROC=volume"

	## SoundWire topologies
	# sof-icl-r700 is kept for compatibility with CI and previous versions of the kernel.
	"sof-icl-rt700\;sof-icl-rt700\;-DCHANNELS=4\;-DPLATFORM=icl\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-icl-rt700\;sof-icl-rt700-4ch\;-DCHANNELS=4\;-DPLATFORM=icl\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-icl-rt700\;sof-icl-rt700-2ch\;-DCHANNELS=2\;-DPLATFORM=icl\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	# sof-cml-r700 is kept for compatibility with CI and previous versions of the kernel.
	"sof-cml-rt700\;sof-cml-rt700\;-DCHANNELS=4\;-DPLATFORM=cml\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-cml-rt700\;sof-cml-rt700-4ch\;-DCHANNELS=4\;-DPLATFORM=cml\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-cml-rt700\;sof-cml-rt700-2ch\;-DCHANNELS=2\;-DPLATFORM=cml\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-cml-rt711-rt1308-mono-rt715\;-DPLATFORM=cml\;-DMONO"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-cml-rt711-rt1308-rt715\;-DPLATFORM=cml"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-icl-rt711-rt1308-rt715\;-DPLATFORM=icl"
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
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-2ch\;-DCHANNELS=2\;-DEXT_AMP\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-rt1308-4ch\;-DCHANNELS=4\;-DEXT_AMP\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-tgl-rt711-4ch\;-DCHANNELS=4\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-4ch\;-DCHANNELS=4\;-DBT_OFFLOAD\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711\;-DCHANNELS=0\;-DBT_OFFLOAD\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-adl-rt711-l0-rt1316-l3-2ch\;-DCHANNELS=2\;-DEXT_AMP\;-DEXT_AMP_REF\;-DAMP_1_LINK=3\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=adl"
	"sof-tgl-rt711-rt1308\;sof-rpl-rt711-4ch\;-DCHANNELS=4\;-DBT_OFFLOAD\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=rpl"
	"sof-tgl-rt711-rt1308\;sof-rpl-rt711\;-DCHANNELS=0\;-DBT_OFFLOAD\;-DPLATFORM=rpl"
	## end SoundWire topologies

	## SoundWire NOJACK topologies
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l2-mono-rt714-l0\;-DPLATFORM=adl\;-DMONO\;-DNOJACK\;-DAMP_1_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l12-rt714-l0\;-DPLATFORM=adl\;-DNOJACK\;-DAMP_1_LINK=1\;-DAMP_2_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=0"
	"sof-icl-rt711-rt1308-rt715-hdmi\;sof-adl-rt1316-l2-mono-rt714-l3\;-DPLATFORM=adl\;-DMONO\;-DNOJACK\;-DAMP_1_LINK=2\;-DEXT_AMP_REF\;-DMIC_LINK=3"
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
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682\;-DCODEC=MAX98360A\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-waves\;-DCODEC=MAX98360A\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DWAVES"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-2way\;-DCODEC=MAX98360A_TDM\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-D2CH_2WAY\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-4ch\;-DCODEC=MAX98360A_TDM\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-D4CH_PASSTHROUGH\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98360a-rt5682-rtnr\;-DCODEC=MAX98360A\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DCHANNELS=2\;-DRTNR"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682-pdm1\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DDMIC_DAI_LINK_16k_PDM=STEREO_PDM1\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682-pdm1-google-aec\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DDMIC_DAI_LINK_16k_PDM=STEREO_PDM1\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DGOOGLE_RTC_AUDIO_PROCESSING"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682-pdm1-drceq\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DDMIC_DAI_LINK_16k_PDM=STEREO_PDM1\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DDRC_EQ"
	"sof-tgl-max98357a-rt5682\;sof-tgl-rt1011-rt5682\;-DCODEC=RT1011\;-DFMT=s24le\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98357a-rt5682\;sof-tgl-max98357a-rt5682-rtnr\;-DCODEC=MAX98357A\;-DFMT=s16le\;-DPLATFORM=tgl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DCHANNELS=2\;-DRTNR"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-rt5682\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-rt5682-google-aec\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DGOOGLE_RTC_AUDIO_PROCESSING"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-rt5682-rtnr\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DCHANNELS=2\;-DRTNR\;-DNOHOTWORD\;-DDMICPROC=rtnr\;-DNO16KDMIC\;-DDMIC_48k_CORE_ID=1"
	"sof-tgl-max98357a-rt5682\;sof-adl-max98390-ssp2-rt5682-ssp0\;-DCODEC=MAX98390\;-DFMT=s32le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=2"
	"sof-tgl-max98357a-rt5682\;sof-adl-rt5682\;-DNO_AMP\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682"
	"sof-tgl-max98357a-rt5682\;sof-adl-rt1019-rt5682\;-DCODEC=RT1019\;-DFMT=s16le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98357a-rt5682\;sof-adl-cs35l41\;-DCODEC=CS35L41\;-DFMT=s24le\;-DPLATFORM=adl\;-DLINUX_MACHINE_DRIVER=sof_ssp_amp\;-DAMP_SSP=1\;-DBT_OFFLOAD\;-DNO_HEADPHONE"
	"sof-tgl-max98373-rt5682\;sof-tgl-max98373-rt5682\;-DAMP_SSP=1"
	"sof-tgl-max98373-rt5682\;sof-tgl-max98373-rt5682-igonr\;-DAMP_SSP=1\;-DIGO"
	"sof-tgl-max98373-rt5682\;sof-tgl-max98373-rt5682-xperi\;-DAMP_SSP=1\;-DINCLUDE_IIR_EQ=1"
	"sof-tgl-max98373-rt5682\;sof-tgl-rt5682-ssp0-max98373-ssp2\;-DAMP_SSP=2"
	"sof-tgl-max98373-rt5682\;sof-tgl-rt5682-ssp0-max98373-ssp2-xperi\;-DAMP_SSP=2\;-DINCLUDE_IIR_EQ=1"
	"sof-adl-nau8825\;sof-adl-nau8825\;-DNO_AMP\;-DBT_OFFLOAD"
	"sof-adl-nau8825\;sof-adl-max98373-nau8825\;-DAMP_SSP=1\;-DSMART_AMP\;-DBT_OFFLOAD"
	"sof-adl-nau8825\;sof-adl-max98373-nau8825-dts\;-DAMP_SSP=1\;-DSMART_AMP\;-DBT_OFFLOAD\;-DDTS=`DTS'"
	"sof-adl-nau8825\;sof-adl-max98360a-nau8825\;-DAMP_SSP=1\;-DBT_OFFLOAD"
	"sof-adl-nau8825\;sof-adl-rt1019-nau8825\;-DAMP_SSP=2"
	"sof-tgl-sdw-max98373-rt5682\;sof-tgl-sdw-max98373-rt5682\;-DCHANNELS=4\;-DPLATFORM=tgl"
	"sof-tgl-sdw-max98373-rt5682\;sof-adl-sdw-max98373-rt5682\;-DCHANNELS=4\;-DPLATFORM=adl"
	"sof-jsl-da7219\;sof-jsl-da7219\;-DPLATFORM=jsl"
	"sof-jsl-da7219\;sof-jsl-da7219-mx98360a\;-DPLATFORM=jsl-dedede"
	"sof-smart-amplifier-nocodec\;sof-smart-amplifier-nocodec"
	"sof-jsl-rt5682\;sof-jsl-rt5682-rt1015\;-DPLATFORM=jsl-rt1015"
	"sof-jsl-rt5682\;sof-jsl-rt5682-rt1015-xperi\;-DPLATFORM=jsl-rt1015\;-DINCLUDE_IIR_EQ=1"
	"sof-jsl-rt5682\;sof-jsl-rt5682-mx98360a\;-DPLATFORM=jsl-dedede"
	"sof-jsl-rt5682\;sof-jsl-cs42l42-mx98360a\;-DPLATFORM=jsl-dedede"

	"sof-glk-es8336\;sof-apl-es8336\;-DPLATFORM=bxt\;-DSSP_NUM=2\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-apl-es8336-ssp0\;-DPLATFORM=bxt\;-DSSP_NUM=0\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-apl-es8336-ssp1\;-DPLATFORM=bxt\;-DSSP_NUM=1\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-apl-es8336-ssp2\;-DPLATFORM=bxt\;-DSSP_NUM=2\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-apl-es8336-ssp5\;-DPLATFORM=bxt\;-DSSP_NUM=5\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-apl-es8336-dmic2ch-ssp0\;-DPLATFORM=bxt\;-DSSP_NUM=0\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-apl-es8336-dmic2ch-ssp1\;-DPLATFORM=bxt\;-DSSP_NUM=1\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-apl-es8336-dmic2ch-ssp2\;-DPLATFORM=bxt\;-DSSP_NUM=2\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-apl-es8336-dmic2ch-ssp5\;-DPLATFORM=bxt\;-DSSP_NUM=5\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-apl-es8336-dmic4ch-ssp0\;-DPLATFORM=bxt\;-DSSP_NUM=0\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-apl-es8336-dmic4ch-ssp1\;-DPLATFORM=bxt\;-DSSP_NUM=1\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-apl-es8336-dmic4ch-ssp2\;-DPLATFORM=bxt\;-DSSP_NUM=2\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-apl-es8336-dmic4ch-ssp5\;-DPLATFORM=bxt\;-DSSP_NUM=5\;-DCHANNELS=4"

	"sof-glk-es8336\;sof-glk-es8336\;-DPLATFORM=bxt\;-DSSP_NUM=2\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-glk-es8336-ssp0\;-DPLATFORM=bxt\;-DSSP_NUM=0\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-glk-es8336-ssp1\;-DPLATFORM=bxt\;-DSSP_NUM=1\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-glk-es8336-ssp2\;-DPLATFORM=bxt\;-DSSP_NUM=2\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-glk-es8336-ssp5\;-DPLATFORM=bxt\;-DSSP_NUM=5\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-glk-es8336-dmic2ch-ssp0\;-DPLATFORM=bxt\;-DSSP_NUM=0\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-glk-es8336-dmic2ch-ssp1\;-DPLATFORM=bxt\;-DSSP_NUM=1\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-glk-es8336-dmic2ch-ssp2\;-DPLATFORM=bxt\;-DSSP_NUM=2\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-glk-es8336-dmic2ch-ssp5\;-DPLATFORM=bxt\;-DSSP_NUM=5\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-glk-es8336-dmic4ch-ssp0\;-DPLATFORM=bxt\;-DSSP_NUM=0\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-glk-es8336-dmic4ch-ssp1\;-DPLATFORM=bxt\;-DSSP_NUM=1\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-glk-es8336-dmic4ch-ssp2\;-DPLATFORM=bxt\;-DSSP_NUM=2\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-glk-es8336-dmic4ch-ssp5\;-DPLATFORM=bxt\;-DSSP_NUM=5\;-DCHANNELS=4"

	"sof-glk-es8336\;sof-jsl-es8336-ssp0\;-DPLATFORM=jsl\;-DSSP_NUM=0\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-jsl-es8336-ssp1\;-DPLATFORM=jsl\;-DSSP_NUM=1\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-jsl-es8336-ssp2\;-DPLATFORM=jsl\;-DSSP_NUM=2\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-jsl-es8336-dmic2ch-ssp0\;-DPLATFORM=jsl\;-DSSP_NUM=0\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-jsl-es8336-dmic2ch-ssp1\;-DPLATFORM=jsl\;-DSSP_NUM=1\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-jsl-es8336-dmic2ch-ssp2\;-DPLATFORM=jsl\;-DSSP_NUM=2\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-jsl-es8336-dmic4ch-ssp0\;-DPLATFORM=jsl\;-DSSP_NUM=0\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-jsl-es8336-dmic4ch-ssp1\;-DPLATFORM=jsl\;-DSSP_NUM=1\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-jsl-es8336-dmic4ch-ssp2\;-DPLATFORM=jsl\;-DSSP_NUM=2\;-DCHANNELS=4"

	"sof-glk-es8336\;sof-cml-es8336-ssp0\;-DPLATFORM=cml\;-DSSP_NUM=0\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-cml-es8336-ssp1\;-DPLATFORM=cml\;-DSSP_NUM=1\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-cml-es8336-ssp2\;-DPLATFORM=cml\;-DSSP_NUM=2\;-DCHANNELS=0"
	"sof-glk-es8336\;sof-cml-es8336-dmic2ch-ssp0\;-DPLATFORM=cml\;-DSSP_NUM=0\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-cml-es8336-dmic2ch-ssp1\;-DPLATFORM=cml\;-DSSP_NUM=1\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-cml-es8336-dmic2ch-ssp2\;-DPLATFORM=cml\;-DSSP_NUM=2\;-DCHANNELS=2"
	"sof-glk-es8336\;sof-cml-es8336-dmic4ch-ssp0\;-DPLATFORM=cml\;-DSSP_NUM=0\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-cml-es8336-dmic4ch-ssp1\;-DPLATFORM=cml\;-DSSP_NUM=1\;-DCHANNELS=4"
	"sof-glk-es8336\;sof-cml-es8336-dmic4ch-ssp2\;-DPLATFORM=cml\;-DSSP_NUM=2\;-DCHANNELS=4"

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

	"sof-glk-es8336\;sof-adl-es8336-ssp1\;-DPLATFORM=adl\;-DSSP_NUM=1\;-DCHANNELS=0"

	"sof-imx8-nocodec\;sof-imx8-nocodec"
	"sof-imx8-cs42888\;sof-imx8-cs42888"
	"sof-imx8-nocodec-sai\;sof-imx8-nocodec-sai"
	"sof-imx8-wm8960\;sof-imx8-wm8960\;-DPPROC=volume"
	"sof-imx8-wm8960\;sof-imx8-eq-iir-wm8960\;-DPPROC=eq-iir-volume"
	"sof-imx8-wm8960\;sof-imx8-eq-fir-wm8960\;-DPPROC=eq-fir-volume"
	"sof-imx8-wm8960\;sof-imx8-drc-wm8960\;-DPPROC=drc"
	"sof-imx8-wm8960-mixer\;sof-imx8-wm8960-mixer"
	"sof-imx8-compr-wm8960-mixer\;sof-imx8-compr-wm8960-mixer"
	"sof-imx8mp-compr-wm8960-mixer\;sof-imx8mp-compr-wm8960-mixer"
	"sof-imx8-wm8960-kwd\;sof-imx8-wm8960-kwd"
	"sof-imx8-cs42888-mixer\;sof-imx8-cs42888-mixer"
	"sof-imx8mp-wm8960-mixer\;sof-imx8mp-wm8960-mixer"
	"sof-imx8mp-wm8960-kwd\;sof-imx8mp-wm8960-kwd"
	"sof-imx8ulp-btsco\;sof-imx8ulp-btsco"
	"sof-imx8ulp-9x9-btsco\;sof-imx8ulp-9x9-btsco"
	"sof-imx8mp-wm8960\;sof-imx8mp-wm8960\;-DPPROC=volume"
	"sof-imx8mp-wm8960\;sof-imx8mp-eq-iir-wm8960\;-DPPROC=eq-iir-volume"
	"sof-imx8mp-wm8960\;sof-imx8mp-eq-fir-wm8960\;-DPPROC=eq-fir-volume"
	"sof-imx8mp-wm8960\;sof-imx8mp-drc-wm8960\;-DPPROC=drc"
	"sof-imx8mp-wm8904\;sof-imx8mp-wm8904\;-DPPROC=volume"

	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-rt1019-rt5682"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-rt1019-rt5682-dts\;-DDTS=`DTS'"
        "sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682-rtnr\;-DCHANNELS=2\;-DRTNR"
	"sof-mt8195-mt6359-rt1019-rt5682\;sof-mt8195-mt6359-max98390-rt5682-google-aec-rtnr\;-DGOOGLE_RTC_AUDIO_PROCESSING\;-DCHANNELS=2\;-DRTNR"
	"sof-mt8186-mt6366\;sof-mt8186"
	"sof-mt8186-mt6366\;sof-mt8186-mt6366-rt1019-rt5682s-waves\;-DWAVES=1"

	"sof-acp-renoir\;sof-acp"
	"sof-rn-rt5682-rt1019\;sof-rn-rt5682-rt1019"
	"sof-rn-rt5682-rt1019\;sof-rn-rt5682-rt1019-dts\;-DDTS=`DTS'"
	"sof-rn-rt5682-max98360\;sof-rn-rt5682-max98360"
	"sof-acp-rmb\;sof-acp-rmb"
	"sof-acp-rmb\;sof-rmb-rt5682s-rt1019"
	"sof-acp-rmb\;sof-rmb-nau8825-max9860"
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
		DEPENDS abi_v1
		VERBATIM
		USES_TERMINAL
	)

	add_alsatplg_command(production/${output}.conf production/${output}.tplg)

	add_custom_target(topology1_${output} DEPENDS production/${output}.tplg)
	add_dependencies(topologies1 topology1_${output})
endforeach()

add_subdirectory(development)
add_subdirectory(dsp_enhancement)
add_subdirectory(kernel_dependent)

# SPDX-License-Identifier: BSD-3-Clause

set(TPLGS
	"sof-apl-asrc-pcm512x\;sof-apl-asrc-pcm512x"
	"sof-apl-asrc-wm8804\;sof-apl-asrc-wm8804"
	"sof-apl-src-pcm512x\;sof-apl-src-pcm512x"
	"sof-apl-dmic-a96k-b16k\;sof-apl-dmic-a96k-b16k"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a2ch-b2ch\;-DDMICSETTING=apl-dmic-a2b2"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a2ch-b4ch\;-DDMICSETTING=apl-dmic-a2b4"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a4ch-b2ch\;-DDMICSETTING=apl-dmic-a4b2"
	"sof-apl-dmic\;sof-apl-dmic-2ch\;-DCHANNELS=2\;-DCPROC=volume"
	"sof-apl-dmic\;sof-apl-dmic-4ch\;-DCHANNELS=4\;-DCPROC=volume"
	"sof-apl-pcm512x-nohdmi\;sof-apl-pcm512x-nohdmi\;-DPPROC=volume"
	"sof-apl-src-50khz-pcm512x\;sof-apl-src-50khz-pcm512x"
	"sof-cnl-rt5682-sdw2\;sof-cnl-rt5682-sdw2\;-DPLATFORM=cnl"
	"sof-cml-src-rt5682\;sof-cml-src-rt5682"
	"sof-hda-asrc\;sof-hda-asrc-2ch\;-DCHANNELS=2"
	"sof-apl-nocodec-ci\;sof-apl-nocodec-ci"
	"sof-tgl-nocodec-ci\;sof-tgl-nocodec-ci"
	"sof-tgl-nocodec-ci\;sof-adl-nocodec-ci"
	"sof-cml-rt1011-rt5682-nokwd\;sof-cml-rt1011-rt5682-nokwd\;-DCHANNELS=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-cml-rt1011-rt5682-nokwd\;sof-cml-rt1011-rt5682-eq\;-DCHANNELS=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DHSEARPROC=eq-iir-eq-fir-volume\;-DHSMICPROC=eq-fir-volume\;-DSPKPROC=eq-iir-eq-fir-volume"
	"sof-cml-rt1011-rt5682-nokwd\;sof-cml-rt1011-rt5682-asrc\;-DCHANNELS=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DHSEARPROC=asrc-volume\;-DHSMICPROC=asrc-volume\;-DSPKPROC=eq-iir-eq-fir-volume"
	"sof-cml-rt1011-rt5682-nokwd\;sof-cml-rt1011-rt5682-tdfb_68mm_2ch\;-DCHANNELS=2\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line2_68mm_azm90_90_13el0_0_13deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line2_68mm_azm90_90_13el0_0_13deg_48khz.m4"
	"sof-imx8mp-src-wm8960\;sof-imx8mp-src-wm8960"
	"sof-imx8-src-wm8960\;sof-imx8-src-wm8960"
	"sof-imx8-src-cs42888\;sof-imx8-src-cs42888"
	"sof-imx8-compr-wm8960\;sof-imx8-compr-wm8960"
	"sof-imx8-compr-pcm-wm8960\;sof-imx8-compr-pcm-wm8960"
	"sof-imx8-compr-pcm-cap-wm8960\;sof-imx8-compr-pcm-cap-wm8960"
	"sof-imx8mp-compr-wm8960\;sof-imx8mp-compr-wm8960"
	"sof-imx8mp-compr-pcm-wm8960\;sof-imx8mp-compr-pcm-wm8960"
	"sof-imx8mp-compr-pcm-cap-wm8960\;sof-imx8mp-compr-pcm-cap-wm8960"
	"sof-apl-nocodec-demux-eq-4ch4ch\;sof-apl-nocodec-demux-eq-4ch4ch"
	"sof-apl-nocodec-demux-eq-2ch4ch\;sof-apl-nocodec-demux-eq-2ch4ch"

	"sof-hda-generic-kwd\;sof-hda-generic-2ch-kwd\;-DCHANNELS=2\;-DDYNAMIC=1"
	"sof-hda-generic-kwd\;sof-hda-generic-4ch-kwd\;-DCHANNELS=4\;-DDYNAMIC=1"
)


# The topologies those are built from topology in the parent directory
set(TPLGS_UP
	"sof-apl-pcm512x\;sof-apl-pcm512x-tdfb_28mm-4ch\;-DFSYNC=48000\;-DDMIC_PCM_CHANNELS=2\;-DDMIC_DAI_CHANNELS=4\;-DDMIC16K_PCM_CHANNELS=2\;-DDMIC16K_DAI_CHANNELS=4\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line4_28mm_azm90_90_13el0_0_13deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line4_28mm_azm90_90_13el0_0_13deg_48khz.m4\;-DDMICPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-cml-rt1011-rt5682\;sof-cml-eq-rt1011-rt5682\;-DPLATFORM=cml\;-DPPROC=eq-iir-eq-fir-volume"
	"sof-cml-rt5682\;sof-cml-eq-fir-loud-rt5682\;-DPLATFORM=cml\;-DHSEARPROC=eq-iir-volume\;-DPIPELINE_FILTER1=eq_iir_coef_loudness.m4\;-DHSMICPROC=eq-fir-volume\;-DPIPELINE_FILTER2=eq_fir_coef_loudness.m4\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-cml-eq-fir-rt5682\;-DPLATFORM=cml\;-DHSMICPROC=eq-fir-volume\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-cml-eq-iir-rt5682\;-DPLATFORM=cml\;-DHSEARPROC=eq-iir-volume\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-glk-da7219\;sof-glk-eq-da7219\;-DHEADPHONE=da7219\;-DDMICPROC=eq-iir-volume"
	"sof-hda-generic\;sof-hda-generic-tdfb_50mm-2ch\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line2_50mm_azm90_90_13el0_0_13deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line2_50mm_azm90_90_13el0_0_13deg_48khz.m4\;-DDMICPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-tdfb_68mm-2ch\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line2_68mm_azm90_90_13el0_0_13deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line2_68mm_azm90_90_13el0_0_13deg_48khz.m4\;-DDMICPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-tdfb_0mm36mm146mm182mm-4ch\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line4_0mm36mm146mm182mm_azm90_90_13el0_0_13deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line4_0mm36mm146mm182mm_azm90_90_13el0_0_13deg_48khz.m4\;-DDMICPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-loud\;-DCHANNELS=0\;-DHSPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DHSPROC_FILTER1=eq_iir_coef_pass.m4\;-DHSPROC_FILTER2=eq_fir_coef_loudness.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-2ch-loud\;-DCHANNELS=2\;-DHSPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DHSPROC_FILTER1=eq_iir_coef_pass.m4\;-DHSPROC_FILTER2=eq_fir_coef_loudness.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-4ch-loud\;-DCHANNELS=4\;-DHSPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DHSPROC_FILTER1=eq_iir_coef_pass.m4\;-DHSPROC_FILTER2=eq_fir_coef_loudness.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-multiband-drc\;-DCHANNELS=0\;-DHSPROC=multiband-drc\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-2ch-multiband-drc\;-DCHANNELS=2\;-DHSPROC=multiband-drc\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-drc\;-DCHANNELS=0\;-DHSPROC=drc\;-DDYNAMIC=1"
	"sof-hda-generic\;sof-hda-generic-2ch-drc\;-DCHANNELS=2\;-DHSPROC=drc\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1"
	"sof-tgl-rt711-rt1308\;sof-tgl-sdw-max98373-rt5682-dmic4ch-ampref\;-DCHANNELS=4\;-DEXT_AMP\;-DEXT_AMP_REF\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
)

add_custom_target(dev_topologies1 ALL)

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
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		COMMAND m4 --fatal-warnings
			${DEFINES}
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../platform/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../
			-I ${CMAKE_CURRENT_BINARY_DIR}/../
			${CMAKE_CURRENT_SOURCE_DIR}/../common/abi.m4
			${CMAKE_CURRENT_SOURCE_DIR}/${input}.m4
			> ${output}.conf
		DEPENDS abi_v1
		VERBATIM
		USES_TERMINAL
	)

	add_alsatplg_command(${output}.conf ${output}.tplg)

	add_custom_target(topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(dev_topologies1 topology_${output})
endforeach()

# Duplicate of above to handle topologies in parent directory
foreach(tplg ${TPLGS_UP})
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
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		COMMAND m4 --fatal-warnings
			${DEFINES}
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../m4
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../platform/common
			-I ${CMAKE_CURRENT_SOURCE_DIR}/../
			-I ${CMAKE_CURRENT_BINARY_DIR}/../
			${CMAKE_CURRENT_SOURCE_DIR}/../common/abi.m4
			${CMAKE_CURRENT_SOURCE_DIR}/../${input}.m4
			> ${output}.conf
		DEPENDS abi_v1
		VERBATIM
		USES_TERMINAL
	)
	add_alsatplg_command(${output}.conf ${output}.tplg)

	add_custom_target(topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(dev_topologies1 topology_${output})
endforeach()

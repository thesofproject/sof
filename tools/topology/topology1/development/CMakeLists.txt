# SPDX-License-Identifier: BSD-3-Clause

set(TPLGS
	## i.MX8 (i.MX8QM and i.MX8QXP) topologies
	"sof-imx8-src-wm8960\;sof-imx8-src-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=src\;-DSAI_INDEX=1"
	"sof-imx8-src-wm8960\;sof-imx8-src-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=src\;-DSAI_INDEX=1"
	"sof-imx8-src-cs42888\;sof-imx8-src-cs42888"
	"sof-imx8-compr-pcm-wm8960\;sof-imx8-compr-pcm-wm8960"
	"sof-imx8-compr-pcm-cap-wm8960\;sof-imx8-compr-pcm-cap-wm8960"
	"sof-imx8-compr-wm8960\;sof-imx8-compr-wm8960\;-DCODEC=wm8960\;-DRATE=48000"
	"sof-imx8-compr-wm8960\;sof-imx8-compr-wm8962\;-DCODEC=wm8962\;-DRATE=48000"
	## end i.MX8 (i.MX8QM and i.MX8QXP) topologies

	## i.MX8MP topologies
	"sof-imx8-src-wm8960\;sof-imx8mp-src-wm8960\;-DCODEC=wm8960\;-DRATE=48000\;-DPPROC=src\;-DSAI_INDEX=3"
	"sof-imx8-src-wm8960\;sof-imx8mp-src-wm8962\;-DCODEC=wm8962\;-DRATE=48000\;-DPPROC=src\;-DSAI_INDEX=3"
	"sof-imx8mp-compr-pcm-wm8960\;sof-imx8mp-compr-pcm-wm8960"
	"sof-imx8mp-compr-pcm-cap-wm8960\;sof-imx8mp-compr-pcm-cap-wm8960"
	"sof-imx8mp-compr-wm8960\;sof-imx8mp-compr-wm8960\;-DCODEC=wm8960\;-DRATE=48000"
	"sof-imx8mp-compr-wm8960\;sof-imx8mp-compr-wm8962\;-DCODEC=wm8962\;-DRATE=48000"
	## end i.MX8MP topologies

	"sof-hda-asrc\;sof-hda-asrc-2ch\;-DCHANNELS=2"
	"sof-tgl-nocodec-ci\;sof-tgl-nocodec-ci"
	"sof-tgl-nocodec-ci\;sof-adl-nocodec-ci"
	"sof-tgl-nocodec-mux\;sof-tgl-nocodec-mux"
	"sof-hda-generic-kwd\;sof-hda-generic-2ch-kwd\;-DCHANNELS=2\;-DDYNAMIC=1"
	"sof-hda-generic-kwd\;sof-hda-generic-4ch-kwd\;-DCHANNELS=4\;-DDYNAMIC=1"
)


# The topologies those are built from topology in the parent directory
set(TPLGS_UP
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
	"sof-hda-generic\;sof-hda-generic-2ch-mfcc\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DDYNAMIC=1\;-DDMIC16KPROC=eq-iir-mfcc"
	"sof-tgl-rt711-rt1308\;sof-tgl-sdw-max98373-rt5682-dmic4ch-ampref\;-DCHANNELS=4\;-DEXT_AMP\;-DEXT_AMP_REF\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DPLATFORM=tgl"
	"sof-hda-generic\;sof-hda-generic-src\;-DCHANNELS=0\;-DHSSFX=src-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-2ch-src\;-DCHANNELS=2\;-DHSSFX=src-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-4ch-src\;-DCHANNELS=4\;-DHSSFX=src-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
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
		DEPENDS abi_v1 ${CMAKE_BINARY_DIR}/topology/topology1/abi.h
		VERBATIM
		USES_TERMINAL
	)

	add_alsatplg_command(${output}.conf ${output}.tplg)

	add_custom_target(dev_topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(dev_topologies1 dev_topology_${output})
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
		DEPENDS abi_v1 ${CMAKE_BINARY_DIR}/topology/topology1/abi.h
		VERBATIM
		USES_TERMINAL
	)
	add_alsatplg_command(${output}.conf ${output}.tplg)

	add_custom_target(topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(dev_topologies1 topology_${output})
endforeach()

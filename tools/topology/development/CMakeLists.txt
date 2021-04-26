# SPDX-License-Identifier: BSD-3-Clause

set(TPLGS
	"sof-apl-asrc-pcm512x\;sof-apl-asrc-pcm512x"
	"sof-apl-asrc-wm8804\;sof-apl-asrc-wm8804"
	"sof-apl-dmic-a96k-b16k\;sof-apl-dmic-a96k-b16k"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a2ch-b2ch\;-DDMICSETTING=apl-dmic-a2b2"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a2ch-b4ch\;-DDMICSETTING=apl-dmic-a2b4"
	"sof-apl-dmic-asymmetric\;sof-apl-dmic-a4ch-b2ch\;-DDMICSETTING=apl-dmic-a4b2"
	"sof-apl-dmic\;sof-apl-dmic-2ch\;-DCHANNELS=2\;-DCPROC=volume"
	"sof-apl-dmic\;sof-apl-dmic-4ch\;-DCHANNELS=4\;-DCPROC=volume"
	"sof-apl-pcm512x-nohdmi\;sof-apl-pcm512x-nohdmi\;-DPPROC=volume"
	"sof-apl-src-50khz-pcm512x\;sof-apl-src-50khz-pcm512x"
	"sof-cml-src-rt5682\;sof-cml-src-rt5682"
	"sof-hda-asrc\;sof-hda-asrc-2ch\;-DCHANNELS=2"
	"sof-tgl-nocodec-ci\;sof-tgl-nocodec-ci"
	"sof-cml-rt1011-rt5682-nokwd\;sof-cml-rt1011-rt5682-nokwd\;-DCHANNELS=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-cml-rt1011-rt5682-nokwd\;sof-cml-rt1011-rt5682-eq\;-DCHANNELS=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DHSEARPROC=eq-iir-eq-fir-volume\;-DHSMICPROC=eq-fir-volume\;-DSPKPROC=eq-iir-eq-fir-volume"
	"sof-cml-rt1011-rt5682-nokwd\;sof-cml-rt1011-rt5682-asrc\;-DCHANNELS=2\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4\;-DHSEARPROC=asrc-volume\;-DHSMICPROC=asrc-volume\;-DSPKPROC=eq-iir-eq-fir-volume"
	"sof-imx8mp-src-wm8960\;sof-imx8mp-src-wm8960"
	"sof-imx8-src-wm8960\;sof-imx8-src-wm8960"
	"sof-imx8-src-cs42888\;sof-imx8-src-cs42888"
)


# The topologies those are built from topology in the parent directory
set(TPLGS_UP
	"sof-apl-pcm512x\;sof-apl-pcm512x-tdfb_28mm-4ch\;-DFSYNC=48000\;-DCHANNELS=4\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line4_28mm_pm10deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line4_28mm_pm10deg_48khz.m4"
	"sof-cml-rt1011-rt5682\;sof-cml-eq-rt1011-rt5682\;-DPLATFORM=cml\;-DPPROC=eq-iir-eq-fir-volume"
	"sof-cml-rt5682\;sof-cml-eq-fir-loud-rt5682\;-DPLATFORM=cml\;-DHSEARPROC=eq-iir-volume\;-DPIPELINE_FILTER1=eq_iir_coef_loudness.m4\;-DHSMICPROC=eq-fir-volume\;-DPIPELINE_FILTER2=eq_fir_coef_loudness.m4\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-cml-eq-fir-rt5682\;-DPLATFORM=cml\;-DHSMICPROC=eq-fir-volume\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-cml-rt5682\;sof-cml-eq-iir-rt5682\;-DPLATFORM=cml\;-DHSEARPROC=eq-iir-volume\;-DDMICPROC=eq-iir-volume\;-DDMIC16KPROC=eq-iir-volume"
	"sof-glk-da7219\;sof-glk-eq-da7219\;-DHEADPHONE=da7219\;-DDMICPROC=eq-iir-volume"
	"sof-hda-generic\;sof-hda-generic-tdfb_50mm-2ch\;-DCHANNELS=2\;-DHSPROC=volume\;-DDMIC16KPROC=tdfb-eq-iir-volume\;-DDMIC16KPROC_FILTER1=tdfb/coef_line2_50mm_pm10deg_16khz.m4\;-DDMICPROC=tdfb-eq-iir-volume\;-DDMICPROC_FILTER1=tdfb/coef_line2_50mm_pm10deg_48khz.m4\;-DDMICPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER2=eq_iir_coef_highpass_40hz_20db_16khz.m4"
)

add_custom_target(dev_topologies ALL)

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
		DEPENDS abi
		VERBATIM
		USES_TERMINAL
	)

# This use of VERBOSE relies on original CMake behavior.
# From the add_custom_command() manual:
#
#    Use of VERBATIM is recommended as it enables correct behavior.
#    When VERBATIM is not given the behavior is platform specific because
#    there is no protection of tool-specific special characters.
#
# This is fine because:
# - We don't expect alsatplg to work on Windows any time soon.
# - CMake is too afraid to remove the original, no-VERBATIM behavior:
#      https://gitlab.kitware.com/cmake/cmake/issues/18849
#
# Also note that in alsa-utils commit v1.2.2~15-gcbabe7a3f0cc, alsatplg
# (accidentally?) started ignoring the verbosity level, now it's just
# verbose or not.
	add_custom_command(
		OUTPUT ${output}.tplg
		COMMAND alsatplg \$\${VERBOSE:+-v 1} -c ${output}.conf -o ${output}.tplg
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		USES_TERMINAL
	)

	add_custom_target(topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(dev_topologies topology_${output})
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
		DEPENDS abi
		VERBATIM
		USES_TERMINAL
	)
	add_custom_command(
		OUTPUT ${output}.tplg
		COMMAND alsatplg \$\${VERBOSE:+-v 1} -c ${output}.conf -o ${output}.tplg
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		USES_TERMINAL
	)

	add_custom_target(topology_${output} DEPENDS ${output}.tplg)
	add_dependencies(dev_topologies topology_${output})
endforeach()

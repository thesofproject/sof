# SPDX-License-Identifier: BSD-3-Clause

# The topologies are defined in parent directory and enhanced with end-point specific processing
set(TPLGS_UP
	"sof-hda-generic\;sof-hda-generic-eq\;-DCHANNELS=0\;-DHSPROC=eq-iir-eq-fir-volume\;-DHSPROC_FILTER1=eq_iir_coef_pass.m4\;-DHSPROC_FILTER2=eq_fir_coef_pass.m4"
	"sof-hda-generic\;sof-hda-generic-eq-2ch\;-DCHANNELS=2\;-DHSPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-eq-4ch\;-DCHANNELS=4\;-DHSPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
)

add_custom_target(dsp_topologies ALL)

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
	add_dependencies(dsp_topologies topology_${output})
endforeach()

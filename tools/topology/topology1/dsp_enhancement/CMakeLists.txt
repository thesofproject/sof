# SPDX-License-Identifier: BSD-3-Clause

# The topologies are defined in parent directory and enhanced with end-point specific processing
set(TPLGS_UP
	"sof-hda-generic\;sof-hda-generic-eq\;-DCHANNELS=0\;-DHSPROC=eq-iir-eq-fir-volume\;-DHSPROC_FILTER1=eq_iir_coef_pass.m4\;-DHSPROC_FILTER2=eq_fir_coef_pass.m4"
	"sof-hda-generic\;sof-hda-generic-eq-2ch\;-DCHANNELS=2\;-DHSPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
	"sof-hda-generic\;sof-hda-generic-eq-4ch\;-DCHANNELS=4\;-DHSPROC=eq-iir-eq-fir-volume\;-DDMICPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_48khz.m4\;-DDMIC16KPROC_FILTER1=eq_iir_coef_highpass_40hz_20db_16khz.m4"
)

add_custom_target(dsp_topologies1 ALL)

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
	add_dependencies(dsp_topologies1 topology_${output})
endforeach()

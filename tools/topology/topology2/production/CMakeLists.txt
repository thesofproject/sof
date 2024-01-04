# SPDX-License-Identifier: BSD-3-Clause

include(tplg-targets-hda-generic.cmake)
include(tplg-targets-cavs25.cmake)
include(tplg-targets-ace1.cmake)
include(tplg-targets-ace2.cmake)

add_custom_target(topology2_prod)

foreach(tplg ${TPLGS})
	set(defines "")
	list(LENGTH tplg length)
	list(GET tplg 0 input)
	list(GET tplg 1 output)

	math(EXPR last_index "${length}-1")

	# Get the pre-processor definitions from the 3rd item in the list for each topology
	# ex: "hda-generic\;hda-generic-2ch\;HDA_CONFIG=gain,DMIC_CH=2", "defines" would contain "HDA_CONFIG=gain,DMIC_CH=2"
	if (${last_index} EQUAL 2)
		list(GET tplg ${last_index} defines)
	endif()

	add_alsatplg2_command("${CMAKE_CURRENT_BINARY_DIR}/../abi.conf" abi_target
	  "${CMAKE_CURRENT_SOURCE_DIR}/../${input}" "${output}"
	  "${CMAKE_CURRENT_SOURCE_DIR}/../" "${defines}")

	add_custom_target(topology2_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2_prod topology2_${output})
endforeach()

# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
set(TPLGS
# CAVS HDMI only topology with passthrough pipelines
"cavs-passthrough-hdmi\;cavs-passthrough-hdmi"
# CAVS HDA topology with passthrough pipelines
"cavs-passthrough-hdmi\;cavs-passthrough-hda\;HDA_CONFIG=passthrough"
# CAVS HDMI only topology with gain-based pipelines
"cavs-gain-hdmi\;cavs-gain-hdmi"
# CAVS HDA topology with gain-based pipelines
"cavs-gain-hdmi\;cavs-gain-hda\;HDA_CONFIG=gain"
# CAVS HDA topology with mixer-based pipelines
"cavs-passthrough-hdmi\;cavs-mixin-mixout-hda\;HDA_CONFIG=mix"
# CAVS SDW topology with passthrough pipelines
"cavs-sdw\;cavs-sdw"
# CAVS SSP topology with passthrough pipelines
"cavs-nocodec\;cavs-nocodec"
)

# This will override the topology1 binaries with topology2 binaries
add_custom_target(topology2_cavs)
add_dependencies(topologies2 topology2_cavs)

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

	# copy ABI and input conf file contents
	#
	# BUG: move this from CMake configuration time to build time to
	# fix incremental builds
	configure_file(${CMAKE_CURRENT_BINARY_DIR}/../abi.conf ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${input}.conf CONTENTS)
	file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf "${CONTENTS}")

	add_alsatplg2_command("${output}.conf" "${output}.tplg" "${CMAKE_CURRENT_SOURCE_DIR}" "${defines}")
	add_custom_target(topology2_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2_cavs topology2_${output})
endforeach()

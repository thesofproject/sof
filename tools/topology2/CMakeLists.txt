# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;"
# Array of "input-file-name;output-file-name;"
set(TPLGS
	"sof-cnl-nocodec\;sof-cnl-nocodec\;"
	"cavs-nocodec\;cavs-nocodec\;"
	"sof-hda-generic-4ch\;sof-hda-generic-4ch\;"
	"sof-hda-generic-2ch\;sof-hda-generic-2ch\;"
	"sof-tgl-max98373-rt5682\;sof-tgl-max98373-rt5682\;"
	"sof-tgl-sdw-max98373-rt5682-2ch\;sof-tgl-sdw-max98373-rt5682-2ch\;"
	"sof-tgl-sdw-max98373-rt5682-4ch\;sof-tgl-sdw-max98373-rt5682-4ch\;"
)

add_custom_target(topology2 ALL)

foreach(tplg ${TPLGS})
	list(GET tplg 0 input)
	list(GET tplg 1 output)
# Note: this does NOT use VERBATIM, see explanation in ../topology/CMakeLists.txt

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
			${CMAKE_CURRENT_SOURCE_DIR}/${input}.conf > ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		USES_TERMINAL
	)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.tplg
		COMMAND alsatplg \$\${VERBOSE:+-v 1} -c ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf -o ${output}.tplg
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		USES_TERMINAL
	)

	add_custom_target(topology2_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2 topology2_${output})
endforeach()

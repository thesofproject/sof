# SPDX-License-Identifier: BSD-3-Clause

add_custom_target(abi2
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
	DEPENDS ${TPLG_DEPS}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM
	USES_TERMINAL
)

# Array of "input-file-name;output-file-name;"
set(TPLGS
	"cavs-nocodec\;cavs-nocodec\;"
)

add_custom_target(topology2 ALL)

foreach(tplg ${TPLGS})
	list(GET tplg 0 input)
	list(GET tplg 1 output)
# Note: this does NOT use VERBATIM, see explanation in ../topology/CMakeLists.txt
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.tplg
		COMMAND alsatplg \$\${VERBOSE:+-v 1} -p -c ${CMAKE_CURRENT_SOURCE_DIR}/${input}.conf -o ${output}.tplg
		DEPENDS abi2
		USES_TERMINAL
	)

	add_custom_target(topology2_${output} DEPENDS ${output}.tplg)
	add_dependencies(topology2 topology2_${output})
endforeach()

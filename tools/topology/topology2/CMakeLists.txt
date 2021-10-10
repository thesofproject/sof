# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;machine quirks"
# machine quirks are defined as follows:
set(TPLGS
	"sof-cml-rt5682-kwd\;sof-cml-rt5682-kwd\;DYNAMIC_PIPELINE 0"
	"sof-cml-rt1011-rt5682\;sof-cml-rt1011-rt5682\;RT5628-KWD_INCLUDE sof-cml-rt5682-kwd.conf\;DYNAMIC_PIPELINE 0"
)

add_custom_target(topologies2 ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory . ${SOF_TOPOLOGY_BINARY_DIRECTORY}
)

add_dependencies(topologies2 topologies1)

# generate ABI object. Only need to do this once for all topologies
execute_process(
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/abi.conf
)

foreach(tplg ${TPLGS})
	set(machine_quirks "")
	list(LENGTH tplg length)
	list(GET tplg 0 input)
	list(GET tplg 1 output)

	# define variables for configuring input conf files with the appropriate values for machine quirks
	math(EXPR last_index "${length}-1")
	foreach(i RANGE 2 ${last_index})
		list(GET tplg ${i} quirk)
		string(REPLACE " " ";" quirk_pair ${quirk})
		list(GET quirk_pair 0 quirk_name)
		list(GET quirk_pair 1 quirk_value)
		set(${quirk_name} ${quirk_value})
	endforeach(i)

	# copy ABI and input conf file contents
	configure_file(${CMAKE_CURRENT_BINARY_DIR}/abi.conf ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${input}.conf CONTENTS)
	file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf "${CONTENTS}")

	# copy sof-rt5628-kwd.conf include file if needed
	if(DEFINED RT5628-KWD_INCLUDE)
		file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${RT5628-KWD_INCLUDE} CONTENTS)
		file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf "${CONTENTS}")
	endif(DEFINED RT5628-KWD_INCLUDE)

	# copy SSP include file if needed
	if(DEFINED SSP_INCLUDE)
		file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${SSP_INCLUDE} CONTENTS)
		file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf "${CONTENTS}")
	endif(DEFINED SSP_INCLUDE)

	# copy EXT_AMP include file if needed
	if(DEFINED EXT_AMP_INCLUDE)
		file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${EXT_AMP_INCLUDE} CONTENTS)
		file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf "${CONTENTS}")
	endif(DEFINED EXT_AMP_INCLUDE)

	# configure input file
	configure_file(${CMAKE_CURRENT_BINARY_DIR}/${output}_abi.conf ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf @ONLY)

# Note: this does NOT use VERBATIM, see explanation in ../topology/CMakeLists.txt
	add_custom_command(
		OUTPUT ${output}.tplg
		# -p to process Topology2.0 conf file
		COMMAND alsatplg \$\${VERBOSE:+-v 1} -p -c ${output}.conf -o ${output}.tplg
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.conf
		USES_TERMINAL
	)

	add_custom_target(topology2_${output} DEPENDS ${output}.tplg)
	add_dependencies(topologies2 topology2_${output})
endforeach()

# SPDX-License-Identifier: BSD-3-Clause
add_custom_target(topologies2
	COMMAND ${CMAKE_COMMAND} -E copy_directory cavs ${SOF_TOPOLOGY_BINARY_DIRECTORY}
)

# generate ABI
execute_process(
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY}
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/abi.conf
)

# check alsatplg version and built topology2 if alsatplg version is 1.2.6 or greater
# This will override any common topologies built with topology1
execute_process(COMMAND alsatplg --version RESULT_VARIABLE STATUS OUTPUT_VARIABLE ALSA_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
if(STATUS AND NOT STATUS EQUAL 0)
	message(WARNING "alsatplg error: ${STATUS}, topology2 will be skipped")
else()
	string(REPLACE "\n" ";" ALSA_VERSION_LIST ${ALSA_VERSION})
	list(GET ALSA_VERSION_LIST 0 ALSATPLG_VERSION)
	string(REGEX MATCH "[0-9]\.[0-9]\.*[0-9]*" ALSATPLG_VERSION_NUMBER ${ALSATPLG_VERSION})
	if(${ALSATPLG_VERSION_NUMBER} VERSION_LESS "1.2.6")
		message(WARNING "topology2 will be skipped. Minimum required version for alsatplg: 1.2.6")
	else()
		add_dependencies(topologies topologies2)

		# make sure topology1 always builds first so that the installed binaries from v2 override the ones from v1
		add_dependencies(topologies2 topologies1)

	endif()
endif()

add_subdirectory(cavs)

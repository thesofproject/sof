# SPDX-License-Identifier: BSD-3-Clause
add_custom_target(topologies2)

# Check alsatplg version and build topology2 if alsatplg version is
# 1.2.7 or greater, see https://github.com/thesofproject/sof/issues/5323

function(alsatplg_version  OUT_STATUS  OUT_VERSION)
	execute_process(COMMAND alsatplg --version
	  RESULT_VARIABLE status
	  OUTPUT_VARIABLE stdout
	  OUTPUT_STRIP_TRAILING_WHITESPACE)
	message(DEBUG "alsatplg --version: status=${status}, output=${stdout}")

	set(${OUT_STATUS} "${status}" PARENT_SCOPE)

        # Some error messages have already been printed on stderr
	if(NOT status EQUAL 0)
	  message(WARNING "alsatplg --version returned status: ${status},
${stdout}")
	  return()
	endif()

	string(REPLACE "\n" ";" ALSA_VERSION_LIST ${stdout})
	list(GET ALSA_VERSION_LIST 0 ALSATPLG_VERSION)
	string(REGEX MATCH "[0-9]\.[0-9]\.*[0-9]*" ALSATPLG_VERSION_NUMBER ${ALSATPLG_VERSION})

	set(${OUT_VERSION} "${ALSATPLG_VERSION_NUMBER}" PARENT_SCOPE)
endfunction()

alsatplg_version(STATUS ALSATPLG_VERSION_NUMBER)

if(NOT STATUS EQUAL 0)
	message(WARNING "alsatplg error: ${STATUS}; topology2 will be skipped")
else()
	if(${ALSATPLG_VERSION_NUMBER} VERSION_LESS "1.2.7")
		message(WARNING "topology2 skipped. Minimum alsatplg version 1.2.7,\
 found ${ALSATPLG_VERSION_NUMBER}.")
	else()
		add_dependencies(topologies topologies2)
	endif()
endif()

# generate ABI for IPC4
add_custom_command(OUTPUT abi.conf
	COMMAND > abi.conf  ${CMAKE_CURRENT_SOURCE_DIR}/get_abi.sh ${SOF_ROOT_SOURCE_DIRECTORY} "ipc4"
	DEPENDS ${SOF_ROOT_SOURCE_DIRECTORY}/src/include/kernel/abi.h
)
add_custom_target(abi_target
	DEPENDS abi.conf
)

add_dependencies(topologies2 topology2_cavs topology2_ace topology2_dev)
add_subdirectory(avs-tplg)
add_subdirectory(development)
add_subdirectory(sof-ace-tplg)

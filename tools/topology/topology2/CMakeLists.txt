# SPDX-License-Identifier: BSD-3-Clause

# 3.17 required: foreach(<loop_var>... IN ZIP_LISTS <lists>)
cmake_minimum_required(VERSION 3.17)

add_custom_target(topologies2 ALL
    # Create directory structure to be provided for deployment
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/target/sof-ipc4-tplg
    COMMAND ${CMAKE_COMMAND} -E create_symlink sof-ipc4-tplg ${CMAKE_CURRENT_BINARY_DIR}/target/sof-ace-tplg
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/target/development
    # copy the topology files only to target
    COMMAND ${CMAKE_COMMAND} -E copy_if_different production/*.tplg ${CMAKE_CURRENT_BINARY_DIR}/target/sof-ipc4-tplg/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different development/*.tplg ${CMAKE_CURRENT_BINARY_DIR}/target/development/
)

# Check alsatplg version and build topology2 if alsatplg version is
# 1.2.7 or greater, see https://github.com/thesofproject/sof/issues/5323
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

add_dependencies(topologies2 topology2_dev topology2_prod)
add_subdirectory(development)
add_subdirectory(production)

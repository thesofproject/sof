# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_PLUGIN C)

include(../../scripts/cmake/misc.cmake)
include(CheckCCompilerFlag)

set(sof_source_directory "${PROJECT_SOURCE_DIR}/../..")
set(sof_install_directory "${PROJECT_BINARY_DIR}/sof_ep/install")
set(sof_binary_directory "${PROJECT_BINARY_DIR}/sof_ep/build")

set(config_h ${sof_binary_directory}/library_autoconfig.h)

set(parser_source_directory "${PROJECT_SOURCE_DIR}/../tplg_parser")
set(parser_install_dir "${PROJECT_BINARY_DIR}/sof_parser/install")

message("ipc4 build")
set(tplg_ipc plugin_ipc4_defconfig)

include(ExternalProject)

# External project for SOF library
ExternalProject_Add(sof_ep
	DOWNLOAD_COMMAND ""
	SOURCE_DIR "${sof_source_directory}"
	PREFIX "${PROJECT_BINARY_DIR}/sof_ep"
	BINARY_DIR "${sof_binary_directory}"
	CMAKE_ARGS -DCONFIG_LIBRARY=ON
		-DCMAKE_INSTALL_PREFIX=${sof_install_directory}
		-DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
		-DINIT_CONFIG=${tplg_ipc}
		-DCONFIG_H_PATH=${config_h}
		-DCONFIG_LIBRARY_STATIC=ON
		-DOPTIMIZE_FOR_DEBUG=ON
	BUILD_ALWAYS 1
	BUILD_BYPRODUCTS "${sof_install_directory}/lib/libsof.a"
)

add_library(sof_library STATIC IMPORTED)
set_target_properties(sof_library PROPERTIES IMPORTED_LOCATION "${sof_install_directory}/lib/libsof.a")
add_dependencies(sof_library sof_ep)

# External project for topology parser
ExternalProject_Add(parser_ep
	SOURCE_DIR "${parser_source_directory}"
	PREFIX "${PROJECT_BINARY_DIR}/sof_parser"
	BINARY_DIR "${PROJECT_BINARY_DIR}/sof_parser/build"
	CMAKE_ARGS -DCONFIG_LIBRARY=ON
		-DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/sof_parser/install
		-DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
		-DCONFIG_LIBRARY_STATIC=ON
		-DOPTIMIZE_FOR_DEBUG=ON
	BUILD_ALWAYS 1
	BUILD_BYPRODUCTS "${parser_install_dir}/lib/libsof_tplg_parser.a"
)

add_library(sof_parser_lib STATIC IMPORTED)
set_target_properties(sof_parser_lib PROPERTIES IMPORTED_LOCATION "${parser_install_dir}/lib/libsof_tplg_parser.a")
add_dependencies(sof_parser_lib parser_ep)

add_subdirectory(alsaconf)
add_subdirectory(alsaplug)
add_subdirectory(modules)
add_subdirectory(pipe)

add_dependencies(asound_module_pcm_sof sof_parser_lib)
add_dependencies(asound_module_ctl_sof sof_parser_lib)

add_dependencies(sof-pipe sof_mod_shm)
add_dependencies(sof-pipe sof_mod_alsa)

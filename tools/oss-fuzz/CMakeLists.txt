# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)

project(SOF_FUZZ_IPC C)

include(../../scripts/cmake/misc.cmake)

add_executable(fuzz_ipc
	fuzz_ipc.c
)

sof_append_relative_path_definitions(fuzz_ipc)

set(sof_source_directory "${PROJECT_SOURCE_DIR}/../..")
set(sof_install_directory "${PROJECT_BINARY_DIR}/sof_ep/install")
set(sof_binary_directory "${PROJECT_BINARY_DIR}/sof_ep/build")

set(config_h ${sof_binary_directory}/library_autoconfig.h)

target_compile_options(fuzz_ipc PRIVATE -g -O3 -Wall -Werror -Wmissing-prototypes
  -Wimplicit-fallthrough -DCONFIG_LIBRARY -imacros${config_h})

target_link_libraries(fuzz_ipc PRIVATE -ldl -lm)

install(TARGETS fuzz_ipc DESTINATION bin)

if(NOT DEFINED ENV{OUT})
	message(FATAL_ERROR
		"Missing key env vars, please only build with oss-fuzz, see README in tools/oss-fuzz")
endif()

include(ExternalProject)

ExternalProject_Add(sof_ep
	DOWNLOAD_COMMAND ""
	SOURCE_DIR "${sof_source_directory}"
	PREFIX "${PROJECT_BINARY_DIR}/sof_ep"
	BINARY_DIR "${sof_binary_directory}"
	CMAKE_ARGS -DCONFIG_LIBRARY=ON
		-DCONFIG_LIBRARY_STATIC=ON
		-DCMAKE_INSTALL_PREFIX=${sof_install_directory}
		-DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
		-DCONFIG_H_PATH=${config_h}
		-DINIT_CONFIG=library_defconfig
	BUILD_ALWAYS 1
	BUILD_BYPRODUCTS "${sof_install_directory}/lib/libsof.so"
)

add_library(sof_library STATIC IMPORTED)
set_target_properties(sof_library PROPERTIES IMPORTED_LOCATION "${sof_install_directory}/lib/libsof.a")
add_dependencies(sof_library sof_ep)

target_link_libraries(fuzz_ipc PRIVATE sof_library)
target_include_directories(fuzz_ipc PRIVATE ${sof_install_directory}/include)
target_link_options(fuzz_ipc PUBLIC $ENV{LIB_FUZZING_ENGINE})
set_target_properties(fuzz_ipc PROPERTIES RUNTIME_OUTPUT_DIRECTORY $ENV{OUT})

set_target_properties(fuzz_ipc
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)

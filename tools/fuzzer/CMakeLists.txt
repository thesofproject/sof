# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_FUZZER C)

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
	message(FATAL_ERROR
		" In-source builds are not supported.\n"
		" Please remove CMakeCache.txt and the CMakeFiles directory.\n"
		" Then specify a build directory. Example: cmake -Bbuild ..."
	)
endif()

include(../../scripts/cmake/misc.cmake)

include(ExternalProject)

set(parser_src_dir "${PROJECT_SOURCE_DIR}/../tplg_parser")
set(parser_install_dir "${PROJECT_BINARY_DIR}/sof_parser/install")

add_executable(sof-fuzzer
	main.c
	qemu-bridge.c
	topology.c
	platform/byt-host.c
	platform/hsw-host.c
	platform/imx8-host.c
)

sof_append_relative_path_definitions(sof-fuzzer)

target_compile_options(sof-fuzzer PRIVATE
	-g -O -Wall -Werror -Wl,-EL -fPIC -DPIC
	-Wmissing-prototypes -Wimplicit-fallthrough
	-DCONFIG_LIBRARY -DCONFIG_IPC_MAJOR_3)

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/../..")

target_include_directories(sof-fuzzer PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/arch/host/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/platform/library/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}"
)

set(config_h ${sof_binary_directory}/library_autoconfig.h)

target_link_libraries(sof-fuzzer PRIVATE sof_parser)
target_include_directories(sof-fuzzer PRIVATE "${parser_install_dir}/include")

ExternalProject_Add(sof_parser_ep
	SOURCE_DIR "${parser_src_dir}"
	PREFIX "${PROJECT_BINARY_DIR}/sof_parser"
	BINARY_DIR "${PROJECT_BINARY_DIR}/sof_parser/build"
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${parser_install_dir}
		-DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
	BUILD_ALWAYS 1
	BUILD_BYPRODUCTS "${parser_install_dir}/lib/libsof_tplg_parser.so"
)

add_library(sof_parser SHARED IMPORTED)
set_target_properties(sof_parser PROPERTIES IMPORTED_LOCATION "${parser_install_dir}/lib/libsof_tplg_parser.so")
add_dependencies(sof_parser sof_parser_ep)

add_dependencies(sof-fuzzer sof_parser)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
target_include_directories(sof-fuzzer PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_libraries(sof-fuzzer PRIVATE ${GLIB_LDFLAGS})

find_library(LIBRT rt)
if(LIBRT)
	target_link_libraries(sof-fuzzer PRIVATE ${LIBRT})
endif()

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/../..")

target_include_directories(sof-fuzzer PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/arch/host/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}"
)

# TODO: fuzzer should not need RTOS headers: FIX.
if (NOT CONFIG_SOF_ZEPHYR_STRICT_HEADERS)
target_include_directories(sof-fuzzer PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/xtos/include"
)
endif()

install(TARGETS sof-fuzzer DESTINATION bin)

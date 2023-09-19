# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_TESTBENCH C)

include(../../scripts/cmake/misc.cmake)
include(CheckCCompilerFlag)

set(default_asoc_h "/usr/include/alsa/sound/uapi/asoc.h")

add_executable(testbench
	testbench.c
	common_test.c
	file.c
	topology.c
)

sof_append_relative_path_definitions(testbench)

target_include_directories(testbench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(sof_source_directory "${PROJECT_SOURCE_DIR}/../..")
set(sof_install_directory "${PROJECT_BINARY_DIR}/sof_ep/install")
set(sof_binary_directory "${PROJECT_BINARY_DIR}/sof_ep/build")

set(config_h ${sof_binary_directory}/library_autoconfig.h)

target_include_directories(testbench PRIVATE "${sof_source_directory}/src/platform/library/include")
target_include_directories(testbench PRIVATE "${sof_source_directory}/src/audio")

# Configuration time, make copy
configure_file(${default_asoc_h} ${CMAKE_CURRENT_BINARY_DIR}/include/alsa/sound/asoc.h)

# Build time
target_include_directories(testbench PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")

# -Wimplicit-fallthrough is preferred, check if it's supported
check_c_compiler_flag(-Wimplicit-fallthrough supports_implicit_fallthrough)
if (supports_implicit_fallthrough)
    set(implicit_fallthrough -Wimplicit-fallthrough)
endif()

target_compile_options(testbench PRIVATE -g -O3 -Wall -Werror -Wl,-EL -Wmissing-prototypes
  ${implicit_fallthrough} -DCONFIG_LIBRARY -DCONFIG_LIBRARY_STATIC -imacros${config_h})

target_link_libraries(testbench PRIVATE -lm)

install(TARGETS testbench DESTINATION bin)

include(ExternalProject)

ExternalProject_Add(sof_ep
	DOWNLOAD_COMMAND ""
	SOURCE_DIR "${sof_source_directory}"
	PREFIX "${PROJECT_BINARY_DIR}/sof_ep"
	BINARY_DIR "${sof_binary_directory}"
	CMAKE_ARGS -DCONFIG_LIBRARY=ON
		-DCMAKE_INSTALL_PREFIX=${sof_install_directory}
		-DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
		-DINIT_CONFIG=library_defconfig
		-DCONFIG_H_PATH=${config_h}
		-DCONFIG_LIBRARY_STATIC=ON
	BUILD_ALWAYS 1
	BUILD_BYPRODUCTS "${sof_install_directory}/lib/libsof.a"
)

add_library(sof_library STATIC IMPORTED)
set_target_properties(sof_library PROPERTIES IMPORTED_LOCATION "${sof_install_directory}/lib/libsof.a")
add_dependencies(sof_library sof_ep)

set(parser_source_directory "${PROJECT_SOURCE_DIR}/../tplg_parser")
set(parser_install_dir "${PROJECT_BINARY_DIR}/sof_parser/install")

#External project for topology parser
ExternalProject_Add(parser_ep
	SOURCE_DIR "${parser_source_directory}"
	PREFIX "${PROJECT_BINARY_DIR}/sof_parser"
	BINARY_DIR "${PROJECT_BINARY_DIR}/sof_parser/build"
	CMAKE_ARGS
		-DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/sof_parser/install
		-DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
		-DCONFIG_LIBRARY_STATIC=ON
	BUILD_ALWAYS 1
	BUILD_BYPRODUCTS "${parser_install_dir}/lib/libsof_tplg_parser.a"
)

add_library(sof_parser_lib STATIC IMPORTED)
set_target_properties(sof_parser_lib PROPERTIES IMPORTED_LOCATION "${parser_install_dir}/lib/libsof_tplg_parser.a")
add_dependencies(sof_parser_lib parser_ep)

add_dependencies(testbench sof_parser_lib)
target_link_libraries(testbench PRIVATE sof_library)
target_link_libraries(testbench PRIVATE sof_parser_lib)
target_link_libraries(testbench PRIVATE m)
target_include_directories(testbench PRIVATE ${sof_install_directory}/include)
target_include_directories(testbench PRIVATE ${parser_install_dir}/include)

set_target_properties(testbench
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)

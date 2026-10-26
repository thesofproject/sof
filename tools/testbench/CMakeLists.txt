# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_TESTBENCH C)

option(TESTBENCH_XTENSA_SIM "Build single-threaded testbench for Xtensa simulator" OFF)
if(CMAKE_C_COMPILER MATCHES "xt-|xtensa-" OR DEFINED ENV{XTENSA_CORE})
	set(TESTBENCH_XTENSA_SIM ON CACHE BOOL "Build single-threaded testbench for Xtensa simulator" FORCE)
endif()

include(../../scripts/cmake/misc.cmake)
include(CheckCCompilerFlag)

if(EXISTS "/usr/include/alsa/sound/uapi/asoc.h")
	set(default_asoc_h "/usr/include/alsa/sound/uapi/asoc.h")
else()
	set(default_asoc_h "/usr/include/sound/asoc.h")
endif()

file(READ "../../src/arch/host/configs/library_defconfig" library_defconfig)

if(library_defconfig MATCHES "CONFIG_IPC_MAJOR_4=y")
    message("-- Selected IPC4 build for testbench")
    set(testbench "sof-testbench4")
else()
    message("-- Selected IPC3 build for testbench")
    set(testbench "sof-testbench3")
endif()

add_executable(${testbench}
	testbench.c
	file.c
	utils.c
	utils_ipc3.c
	utils_ipc4.c
	topology_ipc3.c
	topology_ipc4.c
)
if(NOT TESTBENCH_XTENSA_SIM)
	target_sources(${testbench} PRIVATE remote_server.c)
endif()

sof_append_relative_path_definitions(${testbench})

target_include_directories(${testbench} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(sof_source_directory "${PROJECT_SOURCE_DIR}/../..")
set(sof_install_directory "${PROJECT_BINARY_DIR}/sof_ep/install")
set(sof_binary_directory "${PROJECT_BINARY_DIR}/sof_ep/build")

set(config_h ${sof_binary_directory}/library_autoconfig.h)

target_include_directories(${testbench} PRIVATE "${sof_source_directory}/src/platform/library/include")
target_include_directories(${testbench} PRIVATE "${sof_source_directory}/src/audio")

# Configuration time, make copy
configure_file(${default_asoc_h} ${CMAKE_CURRENT_BINARY_DIR}/include/alsa/sound/asoc.h)

# Build time
target_include_directories(${testbench} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")

# -Wimplicit-fallthrough is preferred, check if it's supported
check_c_compiler_flag(-Wimplicit-fallthrough supports_implicit_fallthrough)
if (supports_implicit_fallthrough)
    set(implicit_fallthrough -Wimplicit-fallthrough)
endif()

target_compile_options(${testbench} PRIVATE -g -O3 -Wall -Werror -Wmissing-prototypes
  ${implicit_fallthrough} -DCONFIG_LIBRARY -imacros${config_h})
if(TESTBENCH_XTENSA_SIM)
	target_compile_definitions(${testbench} PRIVATE CONFIG_XT_SIMULATOR=1)
endif()
if(CMAKE_C_COMPILER MATCHES "xt-|xtensa-")
	target_compile_options(${testbench} PRIVATE
		-U__INT32_TYPE__ -D__INT32_TYPE__=int
		-U__UINT32_TYPE__ "-D__UINT32_TYPE__=unsigned int"
		-Wno-format)
endif()

target_link_libraries(${testbench} PRIVATE -lm)

install(TARGETS ${testbench} DESTINATION bin)

if(NOT TESTBENCH_XTENSA_SIM)
	add_executable(sof-tb-ctl tb_ctl.c)
	target_include_directories(sof-tb-ctl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
	target_compile_options(sof-tb-ctl PRIVATE -g -O3 -Wall -Werror -Wmissing-prototypes
		${implicit_fallthrough})
	install(TARGETS sof-tb-ctl DESTINATION bin)
endif()

include(ExternalProject)

# Should we do this? Only for sof_ep, not for parser_ep?
# https://stackoverflow.com/questions/12021448/how-can-cmake-arguments-be-forwarded-to-externalproject
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
		-DTESTBENCH_XTENSA_SIM=${TESTBENCH_XTENSA_SIM}
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
	BUILD_ALWAYS 1
	BUILD_BYPRODUCTS "${parser_install_dir}/lib/libsof_tplg_parser.a"
)

add_library(sof_parser_lib STATIC IMPORTED)
set_target_properties(sof_parser_lib PROPERTIES IMPORTED_LOCATION "${parser_install_dir}/lib/libsof_tplg_parser.a")
add_dependencies(sof_parser_lib parser_ep)

add_dependencies(${testbench} sof_parser_lib)
target_link_libraries(${testbench} PRIVATE sof_library)
target_link_libraries(${testbench} PRIVATE sof_parser_lib)
target_link_libraries(${testbench} PRIVATE m)
if(NOT TESTBENCH_XTENSA_SIM)
	target_link_libraries(${testbench} PRIVATE pthread)
endif()
target_include_directories(${testbench} PRIVATE ${sof_install_directory}/include)
target_include_directories(${testbench} PRIVATE ${parser_install_dir}/include)

set_target_properties(${testbench}
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)

if(NOT TESTBENCH_XTENSA_SIM)
	enable_testing()
	add_test(NAME testbench_tier1_modules
		 COMMAND ${CMAKE_COMMAND} -E env
			 TB_BIN=$<TARGET_FILE:${testbench}>
			 TB_CTL_BIN=$<TARGET_FILE:sof-tb-ctl>
			 ${CMAKE_CURRENT_SOURCE_DIR}/run_test_suite.sh --tier 1)
	add_test(NAME testbench_tier2_complex_dags
		 COMMAND ${CMAKE_COMMAND} -E env
			 TB_BIN=$<TARGET_FILE:${testbench}>
			 TB_CTL_BIN=$<TARGET_FILE:sof-tb-ctl>
			 ${CMAKE_CURRENT_SOURCE_DIR}/run_test_suite.sh --tier 2)
	add_test(NAME testbench_tier3_ll_dp
		 COMMAND ${CMAKE_COMMAND} -E env
			 TB_BIN=$<TARGET_FILE:${testbench}>
			 TB_CTL_BIN=$<TARGET_FILE:sof-tb-ctl>
			 ${CMAKE_CURRENT_SOURCE_DIR}/run_test_suite.sh --tier 3)
	add_test(NAME testbench_tier4_multicore
		 COMMAND ${CMAKE_COMMAND} -E env
			 TB_BIN=$<TARGET_FILE:${testbench}>
			 TB_CTL_BIN=$<TARGET_FILE:sof-tb-ctl>
			 ${CMAKE_CURRENT_SOURCE_DIR}/run_test_suite.sh --tier 4)
	add_test(NAME testbench_tier5_controls_remote
		 COMMAND ${CMAKE_COMMAND} -E env
			 TB_BIN=$<TARGET_FILE:${testbench}>
			 TB_CTL_BIN=$<TARGET_FILE:sof-tb-ctl>
			 ${CMAKE_CURRENT_SOURCE_DIR}/run_test_suite.sh --tier 5)
endif()

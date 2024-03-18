# Copyright (c) 2023 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smart_amp_test)

SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

set(MODULE "smart_amp_test")
cmake_path(SET SOF_BASE NORMALIZE ${PROJECT_SOURCE_DIR}/../../../..)

add_library(${MODULE} SHARED)

target_sources(${MODULE} PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/../smart_amp_test_ipc4.c
)

sof_append_relative_path_definitions(${MODULE})

target_include_directories(${MODULE} PRIVATE
	"${ZEPHYR_BASE}/include"
	"${ZEPHYR_BASE}/soc/intel/intel_adsp/common/include"
	"${ZEPHYR_BASE}/soc/intel/intel_adsp/ace/include/ace15_mtpm"
	"${ZEPHYR_BASE}/../modules/hal/xtensa/include"
	"${ZEPHYR_BASE}/../modules/hal/xtensa/zephyr/soc/intel_ace15_mtpm"
	"${SOF_BASE}/src/include"
	"${SOF_BASE}/src/arch/xtensa/include"
	"${SOF_BASE}/src/platform/meteorlake/include"
	"${SOF_BASE}/src/platform/intel/ace/include"
	"${SOF_BASE}/src/include/sof/audio/module_adapter/iadk"
	"${SOF_BASE}/zephyr/include"
	"${SOF_BASE}/xtos/include"
	"${SOF_BASE}/tools/rimage/src/include"
	"${PROJECT_BINARY_DIR}/../include/generated"
)

set(MODULE_PROPERTIES HPSRAM_ADDR "0xa06c1000")
set_target_properties(${MODULE} PROPERTIES ${MODULE_PROPERTIES})

set(MODULE_COMPILE_DEF
	__ZEPHYR__=1
	__XTENSA__
	KERNEL
	MAJOR_IADSP_API_VERSION=5
	MIDDLE_IADSP_API_VERSION=0
	MINOR_IADSP_API_VERSION=0
)
target_compile_definitions(${MODULE} PRIVATE ${MODULE_COMPILE_DEF})

target_compile_options(${MODULE} PRIVATE
	-imacros${PROJECT_BINARY_DIR}/../include/generated/autoconf.h
	-save-temps -O2
)

set(MODULE_LINKER_PARAMS -nostdlib -nodefaultlibs)
target_link_options(${MODULE} PRIVATE
	${MODULE_LINKER_PARAMS}
)

add_custom_command(OUTPUT lib${MODULE}_out.so
	DEPENDS ${MODULE}
	COMMAND ${SOF_BASE}scripts/llext_link_helper.py
		-f lib${MODULE}.so -t "0xa06ca000" ${CMAKE_C_COMPILER} --
		${MODULE_LINKER_PARAMS} -shared -fPIC
		-o lib${MODULE}_llext.so $<TARGET_OBJECTS:${MODULE}>
	COMMAND ${CMAKE_STRIP} -R .xt.* -o lib${MODULE}_out.so lib${MODULE}_llext.so
	COMMAND_EXPAND_LISTS
)

add_custom_target(${MODULE}_llext ALL
	DEPENDS lib${MODULE}_out.so
)

add_dependencies(${MODULE} zephyr_interface)

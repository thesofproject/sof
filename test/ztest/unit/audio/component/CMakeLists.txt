# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.
#
# Audio component unit tests converted from CMock to Ztest

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_audio_component)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../..")
set(sof_top_dir ${SOF_ROOT})

include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/kernel/platform/posix/include
	${SOF_ROOT}/src/platform/posix/include
	${PROJECT_BINARY_DIR}/include/generated
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	comp_set_state_ztest.c
	${SOF_ROOT}/src/audio/component.c
)

sof_append_relative_path_definitions(app)

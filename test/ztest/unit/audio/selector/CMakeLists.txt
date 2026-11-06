# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_audio_selector)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../../")
set(sof_top_dir ${SOF_ROOT})

include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
	${SOF_ROOT}/src/audio
	${SOF_ROOT}/tools/rimage/src/include
	${PROJECT_BINARY_DIR}/include/generated
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	selector_ztest.c
	selector_test_support.c
	${SOF_ROOT}/src/audio/selector/selector.c
	${SOF_ROOT}/src/audio/selector/selector_generic.c
	${SOF_ROOT}/src/audio/module_adapter/module_adapter.c
	${SOF_ROOT}/src/audio/module_adapter/module_adapter_ipc4.c
	${SOF_ROOT}/src/audio/module_adapter/module/generic.c
	${SOF_ROOT}/src/audio/buffers/comp_buffer.c
	${SOF_ROOT}/src/audio/buffers/audio_buffer.c
	${SOF_ROOT}/src/audio/source_api_helper.c
	${SOF_ROOT}/src/audio/sink_api_helper.c
	${SOF_ROOT}/src/audio/sink_source_utils.c
	${SOF_ROOT}/src/audio/audio_stream.c
	${SOF_ROOT}/src/audio/component.c
	${SOF_ROOT}/src/audio/data_blob.c
	${SOF_ROOT}/src/module/audio/source_api.c
	${SOF_ROOT}/src/module/audio/sink_api.c
	${SOF_ROOT}/src/ipc/ipc4/helper.c
	${SOF_ROOT}/src/ipc/ipc-common.c
	${SOF_ROOT}/src/ipc/ipc-helper.c
	${SOF_ROOT}/src/lib/objpool.c
	${SOF_ROOT}/src/math/numbers.c
)

# The standalone target compiles the full IPC4 helper translation unit while
# linking only its format helpers; other helper paths leave one local ID unused.
set_source_files_properties(
	${SOF_ROOT}/src/ipc/ipc4/helper.c
	PROPERTIES COMPILE_OPTIONS "-Wno-unused-variable"
)

sof_append_relative_path_definitions(app)

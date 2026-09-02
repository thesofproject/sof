# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_audio_drc)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../../")
set(sof_top_dir ${SOF_ROOT})

# Include SOF CMake helpers used by trace source annotations.
include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
	${SOF_ROOT}/src/audio
	${SOF_ROOT}/test/cmocka/include
	${PROJECT_BINARY_DIR}/include/generated
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DCONFIG_NUMBERS_NORM=1
	-DUNIT_TEST=1
)

target_compile_options(app PRIVATE
	-Wno-error=deprecated-declarations
)

target_sources(app PRIVATE
	drc_process_ztest.c
	drc_test_alloc.c
	${SOF_ROOT}/src/audio/drc/drc.c
	${SOF_ROOT}/src/audio/drc/drc_generic.c
	${SOF_ROOT}/src/audio/drc/drc_math_generic.c
	${SOF_ROOT}/src/audio/drc/drc_math_hifi3.c
	${SOF_ROOT}/src/audio/drc/drc_log.c
	${SOF_ROOT}/src/math/numbers.c
	${SOF_ROOT}/src/math/exp_fcn.c
	${SOF_ROOT}/src/math/exp_fcn_hifi.c
	${SOF_ROOT}/src/math/lut_trig.c
	${SOF_ROOT}/src/audio/module_adapter/module_adapter.c
	${SOF_ROOT}/src/audio/module_adapter/module_adapter_ipc3.c
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
	${SOF_ROOT}/src/ipc/ipc3/helper.c
	${SOF_ROOT}/src/ipc/ipc-common.c
	${SOF_ROOT}/src/ipc/ipc-helper.c
	${SOF_ROOT}/src/lib/objpool.c
	${SOF_ROOT}/src/audio/pipeline/pipeline-graph.c
	${SOF_ROOT}/src/audio/pipeline/pipeline-params.c
	${SOF_ROOT}/src/audio/pipeline/pipeline-schedule.c
	${SOF_ROOT}/src/audio/pipeline/pipeline-stream.c
	${SOF_ROOT}/src/audio/pipeline/pipeline-xrun.c
)

sof_append_relative_path_definitions(app)

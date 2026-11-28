# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.
#
# Audio pipeline unit tests converted from CMock to Ztest

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_audio_pipeline)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../..")
set(sof_top_dir ${SOF_ROOT})

include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/kernel/include
	${SOF_ROOT}/src/kernel/platform/posix/include
	${SOF_ROOT}/src/platform/posix/include
	${PROJECT_BINARY_DIR}/include/generated
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

# Keep full ASan/LSan allocation stacks (twister --enable-asan --enable-lsan)
if(CONFIG_ASAN)
	target_compile_options(app PRIVATE -fno-omit-frame-pointer)
endif()

target_sources(app PRIVATE
	pipeline_new_ztest.c
	pipeline_connect_upstream_ztest.c
	pipeline_free_ztest.c
	pipeline_connection_mocks.c
	${SOF_ROOT}/test/ztest/unit/common/sof_context.c
	${SOF_ROOT}/zephyr/lib/alloc.c
	# sof_context.c needs sys_comp_init()
	${SOF_ROOT}/src/audio/component.c
	${SOF_ROOT}/src/audio/pipeline/pipeline-graph.c
	# pipeline_free() needs pipeline_comp_ll_task_free()
	${SOF_ROOT}/src/audio/pipeline/pipeline-schedule.c
	# pipeline_new() needs ipc_build_stream_posn() and ipc_msg_init(),
	# pipeline_free() needs ipc_msg_list_remove()
	${SOF_ROOT}/src/kernel/ipc/ipc3/helper.c
	${SOF_ROOT}/src/ipc/ipc-common.c
	${SOF_ROOT}/src/kernel/ipc/ipc-common-kernel.c
)

# pipeline_free_ztest.c turns sof_heap_free() into a no-op while its suite runs
target_link_libraries(app PRIVATE "-Wl,--wrap=sof_heap_free")

sof_append_relative_path_definitions(app)

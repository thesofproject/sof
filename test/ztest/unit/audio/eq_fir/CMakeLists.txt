# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.
#
# FIR equalizer processing unit tests converted from CMock to Ztest

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_audio_eq_fir)

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
	${SOF_ROOT}/src/audio
	# cmocka_chirp_2ch.h, still shared with the remaining CMocka tests
	${SOF_ROOT}/test/cmocka/include
	# Reference data, only used by this test
	${SOF_ROOT}/test/cmocka/src/audio/eq_fir
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
	eq_fir_process_ztest.c
	${SOF_ROOT}/test/ztest/unit/common/sof_context.c
	${SOF_ROOT}/zephyr/lib/alloc.c
	${SOF_ROOT}/src/audio/eq_fir/eq_fir.c
	${SOF_ROOT}/src/audio/eq_fir/eq_fir_ipc3.c
	${SOF_ROOT}/src/audio/eq_fir/eq_fir_generic.c
	${SOF_ROOT}/src/math/fir_generic.c
	${SOF_ROOT}/src/math/fir_float.c
	# The HiFi variants compile to nothing on native_sim
	${SOF_ROOT}/src/audio/eq_fir/eq_fir_hifi2ep.c
	${SOF_ROOT}/src/audio/eq_fir/eq_fir_hifi3.c
	${SOF_ROOT}/src/math/fir_hifi2ep.c
	${SOF_ROOT}/src/math/fir_hifi3.c
	${SOF_ROOT}/src/math/numbers.c
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
	${SOF_ROOT}/src/kernel/ipc/ipc3/helper.c
	${SOF_ROOT}/src/ipc/ipc-helper.c
	${SOF_ROOT}/src/lib/objpool.c
)

sof_append_relative_path_definitions(app)

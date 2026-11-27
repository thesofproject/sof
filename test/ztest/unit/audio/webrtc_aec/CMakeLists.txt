# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_audio_webrtc_aec)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../../")
set(sof_top_dir ${SOF_ROOT})

if(NOT DEFINED SOF_WEBRTC_APM_SRC_DIR)
	set(SOF_WEBRTC_APM_SRC_DIR "${SOF_ROOT}/../modules/audio/webrtc-apm")
endif()
cmake_path(NORMAL_PATH SOF_WEBRTC_APM_SRC_DIR)

include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/kernel/include
	${SOF_ROOT}/src/kernel/platform/posix/include
	${SOF_ROOT}/src/platform/posix/include
	${SOF_ROOT}/src/audio
	${SOF_ROOT}/src/audio/webrtc_aec
	${SOF_ROOT}/tools/rimage/src/include
	${SOF_ROOT}/test/cmocka/include
	${PROJECT_BINARY_DIR}/include/generated
	${SOF_WEBRTC_APM_SRC_DIR}
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/include
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/include
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/include
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
	-DNDEBUG=1
)

set(_webrtc_apm_c_srcs
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/echo_cancellation.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_core.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_rdft.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_resampler.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/ring_buffer.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/utility/delay_estimator.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/complex_bit_reverse.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/complex_fft.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/cross_correlation.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/downsample_fast.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/real_fft.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/vector_scaling_operations.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/min_max_operations.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt_floor.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/randomization_functions.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_init.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/division_operations.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2_internal.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_fractional.c
	${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_48khz.c
)

set(_webrtc_aec_c_srcs
	${SOF_ROOT}/src/audio/webrtc_aec/webrtc_aec-aec.c
	${_webrtc_apm_c_srcs}
)

set_source_files_properties(${_webrtc_aec_c_srcs} PROPERTIES
	COMPILE_DEFINITIONS "malloc=webrtc_malloc;calloc=webrtc_calloc;realloc=webrtc_realloc;free=webrtc_free"
	COMPILE_OPTIONS "-Wno-error=deprecated-declarations;-Wno-double-promotion;-Wno-unused-function"
)

target_sources(app PRIVATE
	webrtc_aec_ztest.c
	webrtc_aec_test_alloc.c
	${SOF_ROOT}/test/ztest/unit/common/sof_context.c
	${SOF_ROOT}/src/audio/webrtc_aec/webrtc_aec.c
	${SOF_ROOT}/src/audio/webrtc_aec/webrtc_aec-aec.c
	${SOF_ROOT}/src/audio/webrtc_aec/webrtc_aec-math.c
	${SOF_ROOT}/src/audio/webrtc_aec/webrtc_aec-shims.c
	${_webrtc_apm_c_srcs}
	${SOF_ROOT}/src/audio/module_adapter/module_adapter.c
	${SOF_ROOT}/src/audio/module_adapter/module_adapter_ipc4.c
	${SOF_ROOT}/src/audio/module_adapter/module/generic.c
	${SOF_ROOT}/src/audio/buffers/comp_buffer.c
	${SOF_ROOT}/src/audio/buffers/audio_buffer.c
	${SOF_ROOT}/src/audio/source_api_helper.c
	${SOF_ROOT}/src/audio/sink_api_helper.c
	${SOF_ROOT}/src/audio/sink_source_utils.c
	${SOF_ROOT}/src/math/numbers.c
	${SOF_ROOT}/src/audio/audio_stream.c
	${SOF_ROOT}/src/audio/component.c
	${SOF_ROOT}/src/audio/data_blob.c
	${SOF_ROOT}/src/module/audio/source_api.c
	${SOF_ROOT}/src/module/audio/sink_api.c
	${SOF_ROOT}/src/ipc/ipc4/helper.c
	${SOF_ROOT}/src/ipc/ipc-common.c
	${SOF_ROOT}/src/ipc/ipc-helper.c
	${SOF_ROOT}/src/lib/objpool.c
)

# The standalone target compiles the full IPC4 helper translation unit while
# linking only its format helpers; other helper paths leave one local ID unused.
set_source_files_properties(
	${SOF_ROOT}/src/ipc/ipc4/helper.c
	PROPERTIES COMPILE_OPTIONS "-Wno-unused-variable"
)

sof_append_relative_path_definitions(app)

# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_math_auditory)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../..")
set(sof_top_dir ${SOF_ROOT})

include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
	${SOF_ROOT}/test/ztest/unit/common
	# Reference test vectors are #included in place from the CMocka tree
	# rather than copied, so both frameworks share identical data.
	${SOF_ROOT}/test/cmocka/src/math/auditory
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
	test_auditory_ztest.c
	${SOF_ROOT}/test/ztest/unit/common/mod_alloc.c
	${SOF_ROOT}/zephyr/lib/alloc.c
	${SOF_ROOT}/src/math/auditory/auditory.c
	${SOF_ROOT}/src/math/auditory/mel_filterbank_16.c
	${SOF_ROOT}/src/math/auditory/mel_filterbank_32.c
	${SOF_ROOT}/src/math/log_e.c
	${SOF_ROOT}/src/math/base2log.c
	${SOF_ROOT}/src/math/decibels.c
	${SOF_ROOT}/src/math/numbers.c
)

target_link_libraries(app PRIVATE m)

sof_append_relative_path_definitions(app)

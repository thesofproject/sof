# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_math_dft3)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../../")
set(sof_top_dir ${SOF_ROOT})

include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
	# Reference test vectors are #included in place from the CMocka tree
	# rather than copied, so both frameworks share identical data.
	${SOF_ROOT}/test/cmocka/src/math/fft
	${PROJECT_BINARY_DIR}/include/generated
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	test_dft3_ztest.c
	${SOF_ROOT}/src/math/fft/fft_multi.c
	${SOF_ROOT}/src/math/fft/fft_multi_generic.c
	${SOF_ROOT}/src/math/fft/fft_multi_hifi3.c
	${SOF_ROOT}/src/math/fft/fft_common.c
	${SOF_ROOT}/src/math/fft/fft_32.c
)

target_link_libraries(app PRIVATE m)

sof_append_relative_path_definitions(app)

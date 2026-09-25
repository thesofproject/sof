# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_math_a_law)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../..")

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	# Reference test vectors are #included in place from the CMocka tree
	# rather than copied, so both frameworks share identical data.
	${SOF_ROOT}/test/cmocka/src/math/arithmetic
)

# Define SOF-specific configurations for unit testing
target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	test_a_law_codec_ztest.c
	${SOF_ROOT}/src/math/a_law.c
)

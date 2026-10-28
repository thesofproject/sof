# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_math_window)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../..")

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	# Reference windows are #included in place from the CMocka tree rather
	# than copied, so both frameworks share identical data.
	${SOF_ROOT}/test/cmocka/src/math/window
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	test_window_ztest.c
	${SOF_ROOT}/src/math/window.c
	${SOF_ROOT}/src/math/trig.c
	${SOF_ROOT}/src/math/log_e.c
	${SOF_ROOT}/src/math/base2log.c
	${SOF_ROOT}/src/math/decibels.c
)

target_link_libraries(app PRIVATE m)

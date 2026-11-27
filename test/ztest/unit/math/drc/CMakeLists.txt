# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sof_unit_math_drc)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../../")
set(sof_top_dir ${SOF_ROOT})

include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
	${SOF_ROOT}/src/audio
	${PROJECT_BINARY_DIR}/include/generated
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

# The same sources as the CMocka drc_math_test target. The HiFi3 variants
# are inert on native_sim, they compile only for Xtensa HiFi targets.
target_sources(app PRIVATE
	test_drc_math_ztest.c
	${SOF_ROOT}/src/audio/drc/drc_math_generic.c
	${SOF_ROOT}/src/audio/drc/drc_math_hifi3.c
	${SOF_ROOT}/src/math/numbers.c
	${SOF_ROOT}/src/math/exp_fcn.c
	${SOF_ROOT}/src/math/exp_fcn_hifi.c
)

sof_append_relative_path_definitions(app)

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_math_advanced_pcan)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../../..")

# Set SOF top directory for UUID registry generation
set(sof_top_dir ${SOF_ROOT})

# Include SOF CMake utilities for proper SOF source compilation
include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

# Locate the tflite-micro checkout (west-managed in the SOF workspace)
if(NOT DEFINED TFLM_PATH)
	if(EXISTS "${SOF_ROOT}/../tflite-micro")
		set(TFLM_PATH "${SOF_ROOT}/../tflite-micro")
	elseif(EXISTS "${SOF_ROOT}/../../tflite-micro")
		set(TFLM_PATH "${SOF_ROOT}/../../tflite-micro")
	endif()
endif()

if(NOT DEFINED TFLM_PATH OR NOT EXISTS "${TFLM_PATH}")
	message(FATAL_ERROR
		"tflite-micro not found; expected next to the SOF workspace. "
		"Set TFLM_PATH manually or run 'west update' to fetch it.")
endif()

set(MICROFRONTEND_DIR ${TFLM_PATH}/tensorflow/lite/experimental/microfrontend/lib)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
	${SOF_ROOT}/test/cmocka/include
	${TFLM_PATH}
	${PROJECT_BINARY_DIR}/include/generated
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	pcan_test.c
	${SOF_ROOT}/src/math/pcan/pcan.c
	${SOF_ROOT}/src/math/log_e.c
	${SOF_ROOT}/src/math/base2log.c
	${SOF_ROOT}/src/math/exp_fcn.c
	${SOF_ROOT}/src/math/exp_fcn_hifi.c
	${MICROFRONTEND_DIR}/pcan_gain_control.c
	${MICROFRONTEND_DIR}/noise_reduction.c
	${MICROFRONTEND_DIR}/noise_reduction_util.c
	${MICROFRONTEND_DIR}/log_scale.c
	${MICROFRONTEND_DIR}/log_lut.c
)

sof_append_relative_path_definitions(app)

# SPDX-License-Identifier: BSD-3-Clause

if(NOT DEFINED TFLM_PATH)
  if(EXISTS "${sof_top_dir}/../tflite-micro")
    set(TFLM_PATH "${sof_top_dir}/../tflite-micro")
  elseif(EXISTS "${PROJECT_SOURCE_DIR}/../tflite-micro")
    set(TFLM_PATH "${PROJECT_SOURCE_DIR}/../tflite-micro")
  elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../../../../tflite-micro")
    set(TFLM_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../../../tflite-micro")
  endif()
endif()

set(MICROFRONTEND_DIR ${TFLM_PATH}/tensorflow/lite/experimental/microfrontend/lib)

include_directories(${TFLM_PATH})

cmocka_test(pcan
	pcan_test.c
	${PROJECT_SOURCE_DIR}/src/math/pcan/pcan.c
	${MICROFRONTEND_DIR}/pcan_gain_control.c
	${MICROFRONTEND_DIR}/pcan_gain_control_util.c
	${MICROFRONTEND_DIR}/noise_reduction.c
	${MICROFRONTEND_DIR}/noise_reduction_util.c
)

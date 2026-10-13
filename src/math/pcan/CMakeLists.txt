# SPDX-License-Identifier: BSD-3-Clause

if(NOT DEFINED TFLM_PATH)
  if(EXISTS "${sof_top_dir}/../tflite-micro")
    set(TFLM_PATH "${sof_top_dir}/../tflite-micro")
  elseif(EXISTS "${PROJECT_SOURCE_DIR}/../tflite-micro")
    set(TFLM_PATH "${PROJECT_SOURCE_DIR}/../tflite-micro")
  elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../../tflite-micro")
    set(TFLM_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../tflite-micro")
  endif()
endif()

set(MICROFRONTEND_DIR ${TFLM_PATH}/tensorflow/lite/experimental/microfrontend/lib)

set(base_files
  pcan.c
  ${MICROFRONTEND_DIR}/pcan_gain_control.c
  ${MICROFRONTEND_DIR}/pcan_gain_control_util.c
  ${MICROFRONTEND_DIR}/noise_reduction.c
  ${MICROFRONTEND_DIR}/noise_reduction_util.c
)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  zephyr_include_directories(${TFLM_PATH})
  zephyr_library_sources(
    ${base_files}
  )

else() ### library, e.g. testbench or plugin ###

  include_directories(${TFLM_PATH})
  add_local_sources(sof ${base_files})

endif()

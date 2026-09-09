# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_WEBRTC_AGC_STUB)
  sof_llext_build("webrtc_agc"
    SOURCES ../webrtc_agc.c
            ../webrtc_agc-stub.c
    LIB openmodules
  )
else()
  if(NOT DEFINED SOF_WEBRTC_APM_SRC_DIR)
    set(SOF_WEBRTC_APM_SRC_DIR "${sof_top_dir}/../modules/audio/webrtc-apm"
        CACHE PATH "webrtc-audio-processing 0.3.x source directory")
  endif()
  cmake_path(NORMAL_PATH SOF_WEBRTC_APM_SRC_DIR)

  sof_llext_build("webrtc_agc"
    SOURCES ../webrtc_agc.c
            ../webrtc_agc-agc.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/agc/legacy/digital_agc.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/division_operations.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt_floor.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/downsample_fast.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/min_max_operations.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/vector_scaling_operations.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/copy_set_operations.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/dot_product_with_scale.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2.c
            ${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2_internal.c
    INCLUDES "${SOF_WEBRTC_APM_SRC_DIR}"
             "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/agc/legacy"
             "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/include"
    DEFINITIONS -DWEBRTC_ARCH_32_BITS -DWEBRTC_ARCH_LITTLE_ENDIAN
    LIB openmodules
  )
endif()

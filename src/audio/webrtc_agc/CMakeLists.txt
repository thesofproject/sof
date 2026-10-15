# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_AGC STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_agc_llext)
  add_dependencies(app webrtc_agc)
else()
  add_local_sources(sof webrtc_agc.c)
  if(CONFIG_COMP_WEBRTC_AGC_STUB)
    add_local_sources(sof webrtc_agc-stub.c)
  else()
    if(NOT DEFINED SOF_WEBRTC_APM_SRC_DIR)
      set(SOF_WEBRTC_APM_SRC_DIR "${sof_top_dir}/../modules/audio/webrtc-apm"
          CACHE PATH "webrtc-audio-processing 0.3.x source directory")
    endif()
    cmake_path(NORMAL_PATH SOF_WEBRTC_APM_SRC_DIR)

    zephyr_include_directories(
      "${SOF_WEBRTC_APM_SRC_DIR}"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/agc/legacy"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/include"
    )

    add_local_sources(sof webrtc_agc-agc.c)
    add_local_sources(sof
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/agc/legacy/digital_agc.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt_floor.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/downsample_fast.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/min_max_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/vector_scaling_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/copy_set_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/dot_product_with_scale.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2.c"
    )

    add_local_sources(sof
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/division_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2_internal.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_fractional.c"
    )

    set_source_files_properties(
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/agc/legacy/digital_agc.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt_floor.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/downsample_fast.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/min_max_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/vector_scaling_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/copy_set_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/dot_product_with_scale.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2.c"
      TARGET_DIRECTORY modules_sof
      PROPERTIES COMPILE_OPTIONS "-Wno-double-promotion;-Wno-implicit-function-declaration"
    )
  endif()
endif()

# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_AEC STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_aec_llext)
  add_dependencies(app webrtc_aec)
else()
  add_local_sources(sof webrtc_aec.c)
  if(CONFIG_COMP_WEBRTC_AEC_STUB)
    add_local_sources(sof webrtc_aec-stub.c)
  else()
    if(NOT DEFINED SOF_WEBRTC_APM_SRC_DIR)
      set(SOF_WEBRTC_APM_SRC_DIR "${sof_top_dir}/../modules/audio/webrtc-apm"
          CACHE PATH "webrtc-audio-processing 0.3.x source directory")
    endif()
    cmake_path(NORMAL_PATH SOF_WEBRTC_APM_SRC_DIR)

    zephyr_include_directories(
      "${SOF_WEBRTC_APM_SRC_DIR}"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/include"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/include"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/include"
    )

    zephyr_compile_definitions(
      WEBRTC_ARCH_32_BITS
      WEBRTC_ARCH_LITTLE_ENDIAN
    )

    if(CONFIG_WEBRTC_AEC_BACKEND_FLOAT)
      add_local_sources(sof webrtc_aec-aec.c webrtc_aec-math.c)
      add_local_sources(sof
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/echo_cancellation.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_core.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_rdft.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_resampler.c"
      )
    else()
      add_local_sources(sof webrtc_aec-aecm.c)
      add_local_sources(sof
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/echo_control_mobile.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/aecm_core.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/aecm_core_c.c"
      )
    endif()

    add_local_sources(sof webrtc_aec-shims.c)
    add_local_sources(sof
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/ring_buffer.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/utility/delay_estimator.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/complex_bit_reverse.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/complex_fft.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/cross_correlation.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/downsample_fast.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/real_fft.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/vector_scaling_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/min_max_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt_floor.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/randomization_functions.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_init.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt.c"
    )

    add_local_sources(sof
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/division_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2_internal.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_fractional.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_48khz.c"
    )

    set(_webrtc_aec_srcs
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/ring_buffer.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/utility/delay_estimator.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/complex_bit_reverse.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/complex_fft.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/cross_correlation.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/downsample_fast.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/real_fft.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/vector_scaling_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/min_max_operations.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt_floor.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/randomization_functions.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_init.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_48khz.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_by_2_internal.c"
      "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/signal_processing/resample_fractional.c"
    )

    list(APPEND _webrtc_aec_srcs
      ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec.c
      ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec-shims.c
    )

    if(CONFIG_WEBRTC_AEC_BACKEND_FLOAT)
      list(APPEND _webrtc_aec_srcs
        ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec-aec.c
        ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec-math.c
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/echo_cancellation.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_core.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_rdft.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec/aec_resampler.c"
      )
    else()
      list(APPEND _webrtc_aec_srcs
        ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec-aecm.c
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/echo_control_mobile.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/aecm_core.c"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/aecm_core_c.c"
      )
    endif()

    set_source_files_properties(
      ${_webrtc_aec_srcs}
      TARGET_DIRECTORY modules_sof
      PROPERTIES COMPILE_OPTIONS "-O3;-Xclang;-target-feature;-Xclang;-fp;-Wno-double-promotion;-Wno-implicit-function-declaration;-Wno-unused-function;-Dmalloc=webrtc_malloc;-Dcalloc=webrtc_calloc;-Drealloc=webrtc_realloc;-Dfree=webrtc_free"
    )
  endif()
endif()

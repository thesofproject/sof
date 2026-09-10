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

    if(CONFIG_WEBRTC_AEC_BACKEND_AEC3)
      if(DEFINED CONFIG_SOC_TOOLCHAIN_NAME)
        set(_xtensa_target "${CONFIG_SOC_TOOLCHAIN_NAME}")
      else()
        set(_xtensa_target "intel_ace30_ptl")
      endif()
      set(_sdk_cxx_dir "/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-${_xtensa_target}_zephyr-elf/xtensa-${_xtensa_target}_zephyr-elf")

      target_include_directories(zephyr_interface SYSTEM BEFORE INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:${_sdk_cxx_dir}/include/c++/14.3.0>
        $<$<COMPILE_LANGUAGE:CXX>:${_sdk_cxx_dir}/include/c++/14.3.0/xtensa-${_xtensa_target}_zephyr-elf>
        $<$<COMPILE_LANGUAGE:CXX>:${_sdk_cxx_dir}/include>
      )

      # Do not link heavy libstdc++.a to prevent out-of-range relocations and unwinder bloat.
      # Minimal C++ runtime, new/delete, and exception stubs are provided in webrtc_aec3_shims.cpp.

      add_local_sources(sof
        webrtc_aec-aec3.c
        webrtc_aec-math.c
        webrtc_aec3_wrapper.cpp
        webrtc_aec3_shims.cpp
      )

      file(GLOB _aec3_srcs
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec3/*.cc"
      )
      list(FILTER _aec3_srcs EXCLUDE REGEX ".*_(avx2|neon|mips|sse2)\\.cc$")

      set(_aec3_support_srcs
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/api/audio/echo_canceller3_config.cc"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/common_audio/third_party/ooura/fft_size_128/ooura_fft.cc"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/high_pass_filter.cc"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/utility/cascaded_biquad_filter.cc"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aec3_buffer/audio_buffer.cc"
        "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/logging/apm_data_dumper.cc"
      )

      add_local_sources(sof ${_aec3_srcs} ${_aec3_support_srcs})

      set(_aec3_cxx_all
        ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec3_wrapper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec3_shims.cpp
        ${_aec3_srcs}
        ${_aec3_support_srcs}
      )

      set_source_files_properties(
        ${_aec3_cxx_all}
        TARGET_DIRECTORY modules_sof
        PROPERTIES COMPILE_OPTIONS "-std=c++17;-fno-exceptions;-fno-rtti;-O3;-fno-vectorize;-fno-slp-vectorize;-Xclang;-target-feature;-Xclang;-fp;-DWEBRTC_POSIX;-DWEBRTC_ARCH_GENERIC;-DWEBRTC_APM_DEBUG_DUMP=0;-DRTC_DISABLE_METRICS;-DNDEBUG;-Wno-macro-redefined;-Wno-unused-function;-Wno-double-promotion"
      )
    elseif(CONFIG_WEBRTC_AEC_BACKEND_FLOAT)
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

    if(CONFIG_WEBRTC_AEC_BACKEND_AEC3)
      list(APPEND _webrtc_aec_srcs
        ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec-aec3.c
        ${CMAKE_CURRENT_SOURCE_DIR}/webrtc_aec-math.c
      )
    elseif(CONFIG_WEBRTC_AEC_BACKEND_FLOAT)
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

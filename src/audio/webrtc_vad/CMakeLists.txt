# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_VAD STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_vad_llext)
  add_dependencies(app webrtc_vad)
else()
  add_local_sources(sof webrtc_vad.c)

  if(CONFIG_COMP_WEBRTC_VAD_STUB)
    add_local_sources(sof webrtc_vad-stub.c)
  else()
    if(NOT DEFINED SOF_LIBFVAD_SRC_DIR)
      set(SOF_LIBFVAD_SRC_DIR "${sof_top_dir}/../modules/audio/libfvad"
          CACHE PATH "libfvad source directory")
    endif()
    cmake_path(NORMAL_PATH SOF_LIBFVAD_SRC_DIR)

    zephyr_include_directories(
      "${SOF_LIBFVAD_SRC_DIR}/include"
      "${SOF_LIBFVAD_SRC_DIR}/src"
      "${SOF_LIBFVAD_SRC_DIR}/src/vad"
      "${SOF_LIBFVAD_SRC_DIR}/src/signal_processing"
    )

    add_local_sources(sof webrtc_vad-fvad.c)
    add_local_sources(sof
      "${SOF_LIBFVAD_SRC_DIR}/src/fvad.c"
      "${SOF_LIBFVAD_SRC_DIR}/src/signal_processing/energy.c"
      "${SOF_LIBFVAD_SRC_DIR}/src/signal_processing/get_scaling_square.c"
      "${SOF_LIBFVAD_SRC_DIR}/src/signal_processing/resample_48khz.c"
      "${SOF_LIBFVAD_SRC_DIR}/src/signal_processing/spl_inl.c"
      "${SOF_LIBFVAD_SRC_DIR}/src/vad/vad_core.c"
      "${SOF_LIBFVAD_SRC_DIR}/src/vad/vad_filterbank.c"
      "${SOF_LIBFVAD_SRC_DIR}/src/vad/vad_gmm.c"
      "${SOF_LIBFVAD_SRC_DIR}/src/vad/vad_sp.c"
    )
    if((NOT CONFIG_COMP_WEBRTC_AEC OR CONFIG_COMP_WEBRTC_AEC_STUB) AND
       (NOT CONFIG_COMP_WEBRTC_AGC OR CONFIG_COMP_WEBRTC_AGC_STUB))
      add_local_sources(sof
        "${SOF_LIBFVAD_SRC_DIR}/src/signal_processing/division_operations.c"
        "${SOF_LIBFVAD_SRC_DIR}/src/signal_processing/resample_by_2_internal.c"
        "${SOF_LIBFVAD_SRC_DIR}/src/signal_processing/resample_fractional.c"
      )
    endif()
  endif()
endif()

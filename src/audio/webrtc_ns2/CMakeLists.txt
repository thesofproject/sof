# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_NS2 STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_ns2_llext)
  add_dependencies(app webrtc_ns2)
else()
  add_local_sources(sof webrtc_ns2.c)
  if(CONFIG_COMP_WEBRTC_NS2_STUB)
    add_local_sources(sof webrtc_ns2-stub.c)
  else()
    if(NOT DEFINED SOF_RNNOISE_SRC_DIR)
      set(SOF_RNNOISE_SRC_DIR "${sof_top_dir}/../modules/audio/rnnoise"
          CACHE PATH "RNNoise source directory")
    endif()
    cmake_path(NORMAL_PATH SOF_RNNOISE_SRC_DIR)

    zephyr_include_directories(
      "${SOF_RNNOISE_SRC_DIR}/include"
      "${SOF_RNNOISE_SRC_DIR}/src"
    )

    add_local_sources(sof webrtc_ns2-rnn.c)
    add_local_sources(sof
      "${SOF_RNNOISE_SRC_DIR}/src/denoise.c"
      "${SOF_RNNOISE_SRC_DIR}/src/rnn.c"
      "${SOF_RNNOISE_SRC_DIR}/src/rnn_data.c"
      "${SOF_RNNOISE_SRC_DIR}/src/pitch.c"
      "${SOF_RNNOISE_SRC_DIR}/src/celt_lpc.c"
      "${SOF_RNNOISE_SRC_DIR}/src/kiss_fft.c"
    )
    set_source_files_properties(
      "${SOF_RNNOISE_SRC_DIR}/src/denoise.c"
      "${SOF_RNNOISE_SRC_DIR}/src/rnn.c"
      "${SOF_RNNOISE_SRC_DIR}/src/rnn_data.c"
      "${SOF_RNNOISE_SRC_DIR}/src/pitch.c"
      "${SOF_RNNOISE_SRC_DIR}/src/celt_lpc.c"
      "${SOF_RNNOISE_SRC_DIR}/src/kiss_fft.c"
      TARGET_DIRECTORY modules_sof
      PROPERTIES COMPILE_OPTIONS "-Wno-double-promotion;-Wno-implicit-function-declaration;-Wno-unused-function;-DFLOAT_APPROX"
    )
  endif()
endif()

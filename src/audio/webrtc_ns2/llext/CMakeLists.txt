# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_WEBRTC_NS2_STUB)
  sof_llext_build("webrtc_ns2"
    SOURCES ../webrtc_ns2.c
            ../webrtc_ns2-stub.c
    LIB openmodules
  )
else()
  include(${CMAKE_CURRENT_LIST_DIR}/../webrtc_ns2.cmake)

  sof_llext_build("webrtc_ns2"
    SOURCES ../webrtc_ns2.c
            ../webrtc_ns2-rnn.c
    INCLUDES "${WEBRTC_NS2_INSTALL_DIR}/include"
    LIBS_PATH "${WEBRTC_NS2_INSTALL_DIR}/lib"
    LIBS rnnoise
    LIB openmodules
  )

  add_dependencies(webrtc_ns2_llext_lib rnnoise_ext)
  add_dependencies(webrtc_ns2 rnnoise_ext)
endif()

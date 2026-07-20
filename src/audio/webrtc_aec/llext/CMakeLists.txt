# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_WEBRTC_AEC_STUB)
  sof_llext_build("webrtc_aec"
    SOURCES ../webrtc_aec.c
            ../webrtc_aec-stub.c
    LIB openmodules
  )
else()
  include(${CMAKE_CURRENT_LIST_DIR}/../webrtc_aec.cmake)

  sof_llext_build("webrtc_aec"
    SOURCES ../webrtc_aec.c
            ../webrtc_aec-aecm.c
    INCLUDES "${WEBRTC_AECM_INSTALL_DIR}/include"
    LIBS_PATH "${WEBRTC_AECM_INSTALL_DIR}/lib"
    LIBS webrtc_aecm
    LIB openmodules
  )

  add_dependencies(webrtc_aec_llext_lib webrtc_aecm_ext)
  add_dependencies(webrtc_aec webrtc_aecm_ext)
endif()

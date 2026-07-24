# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_WEBRTC_VAD_STUB)
  # Dependency-free build: SOF glue + passthrough stub backend, no libfvad.
  sof_llext_build("webrtc_vad"
    SOURCES ../webrtc_vad.c
            ../webrtc_vad-stub.c
    LIB openmodules
  )
else()
  # Real VAD: cross-build libfvad and link it.
  include(${CMAKE_CURRENT_LIST_DIR}/../webrtc_vad.cmake)

  sof_llext_build("webrtc_vad"
    SOURCES ../webrtc_vad.c
            ../webrtc_vad-fvad.c
            ../webrtc_vad-shims.c
    INCLUDES "${LIBFVAD_INSTALL_DIR}/include"
    LIBS_PATH "${LIBFVAD_INSTALL_DIR}/lib"
    LIBS fvad
    LIB openmodules
  )

  # The archive must be built before the module compiles/links against it.
  add_dependencies(webrtc_vad_llext_lib fvad_ext)
  add_dependencies(webrtc_vad fvad_ext)
endif()

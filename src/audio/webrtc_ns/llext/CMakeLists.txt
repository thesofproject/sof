# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_WEBRTC_NS_STUB)
  # Dependency-free build: SOF glue + pass-through stub, no WebRTC library.
  sof_llext_build("webrtc_ns"
    SOURCES ../webrtc_ns.c
            ../webrtc_ns-stub.c
    LIB openmodules
  )
else()
  # Real NS: cross-build webrtc-apm NS subset and link it.
  include(${CMAKE_CURRENT_LIST_DIR}/../webrtc_ns.cmake)

  sof_llext_build("webrtc_ns"
    SOURCES ../webrtc_ns.c
            ../webrtc_ns-webrtc.c
            ../webrtc_ns-shims.c
    INCLUDES "${WEBRTC_NS_INSTALL_DIR}/include"
    LIBS_PATH "${WEBRTC_NS_INSTALL_DIR}/lib"
    LIBS webrtc_ns
    LIB openmodules
  )

  add_dependencies(webrtc_ns_llext_lib webrtc_ns_ext)
  add_dependencies(webrtc_ns webrtc_ns_ext)
endif()

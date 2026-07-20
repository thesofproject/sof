# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_NS2 STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_ns2_llext)
  add_dependencies(app webrtc_ns2)
else()
  add_local_sources(sof webrtc_ns2.c)
  if(CONFIG_COMP_WEBRTC_NS2_STUB)
    add_local_sources(sof webrtc_ns2-stub.c)
  else()
    add_local_sources(sof webrtc_ns2-rnn.c)
  endif()
endif()

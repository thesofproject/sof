# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_NS STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_ns_llext)
  add_dependencies(app webrtc_ns)
else()
  add_local_sources(sof webrtc_ns.c)
  if(CONFIG_COMP_WEBRTC_NS_STUB)
    add_local_sources(sof webrtc_ns-stub.c)
  else()
    add_local_sources(sof webrtc_ns-webrtc.c)
  endif()
endif()

# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_AEC STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_aec_llext)
  add_dependencies(app webrtc_aec)
else()
  add_local_sources(sof webrtc_aec.c)
  if(CONFIG_COMP_WEBRTC_AEC_STUB)
    add_local_sources(sof webrtc_aec-stub.c)
  else()
    add_local_sources(sof webrtc_aec-aecm.c)
  endif()
endif()

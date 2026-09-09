# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_HPF STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_hpf_llext)
  add_dependencies(app webrtc_hpf)
else()
  add_local_sources(sof webrtc_hpf.c)
  if(CONFIG_COMP_WEBRTC_HPF_STUB)
    add_local_sources(sof webrtc_hpf-stub.c)
  else()
    add_local_sources(sof webrtc_hpf-filter.c)
  endif()
endif()

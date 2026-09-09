# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_WEBRTC_HPF_STUB)
  sof_llext_build("webrtc_hpf"
    SOURCES ../webrtc_hpf.c
            ../webrtc_hpf-stub.c
    LIB openmodules
  )
else()
  sof_llext_build("webrtc_hpf"
    SOURCES ../webrtc_hpf.c
            ../webrtc_hpf-filter.c
    LIB openmodules
  )
endif()

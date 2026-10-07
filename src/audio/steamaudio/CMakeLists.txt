# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
# Copyright (c) 2026 Intel Corporation. All rights reserved.

if(CONFIG_COMP_STEAMAUDIO STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/steamaudio_llext)
  add_dependencies(app steamaudio)
else()
  add_local_sources(sof steamaudio.c)
  add_local_sources(sof steamaudio-generic.c)
  add_local_sources(sof steamaudio_bvh.c)

  if(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof steamaudio-ipc4.c)
  endif()
endif()

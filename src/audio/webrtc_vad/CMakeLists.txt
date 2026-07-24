# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_WEBRTC_VAD STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/webrtc_vad_llext)
  add_dependencies(app webrtc_vad)
else()
  add_local_sources(sof webrtc_vad.c)

  if(CONFIG_COMP_WEBRTC_VAD_STUB)
    add_local_sources(sof webrtc_vad-stub.c)
  else()
    add_local_sources(sof webrtc_vad-fvad.c)
    # NOTE: the libc symbols needed by libfvad (malloc/free/memset/...) are
    # already provided by the SOF core in built-in and testbench builds.
    # For LLEXT the shims are in llext/CMakeLists.txt (webrtc_vad-shims.c).
  endif()
endif()

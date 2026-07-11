# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_FFMPEG_DEC STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/ffmpeg_dec_llext)
  add_dependencies(app ffmpeg_dec)
else()
  add_local_sources(sof ffmpeg_dec.c)

  if(CONFIG_COMP_FFMPEG_DEC_STUB)
    add_local_sources(sof ffmpeg_dec-stub.c)
  else()
    add_local_sources(sof ffmpeg_dec-ffmpeg.c)
    # NOTE: ffmpeg_dec-shims.c / ffmpeg_dec-alloc.c are LLEXT-only (see
    # llext/CMakeLists.txt). They provide libc/libm symbols the isolated LLEXT
    # cannot import; in a built-in (=y) or native/testbench build the real libc
    # provides those, so including them here would multiply-define malloc etc.
  endif()
endif()

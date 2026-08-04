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
    # Scalar soft-float for the SOF glue that touches float. core-isa.h reports
    # XCHAL_HAVE_FP=1 so LLVM would lower the FLT/FLTP -> Q1.31 sample conversion
    # in ffmpeg_dec_emit_frame() to hardware FP0 ops (lsi/mul.s/trunc.s), which
    # fault EXCCAUSE 0 (illegal instruction) on this CONFIG_FPU=n image -- the FP0
    # coprocessor is never enabled. Disable the fp feature so LLVM emits
    # soft-float libcalls, exactly as ffmpeg.cmake does for FFmpeg/libm. FLAC/MP3
    # never hit this (their emit path is fixed-point); AAC/Opus (FLT/FLTP) do.
    # NB: modules_sof (the library that compiles these) is created in
    # zephyr/CMakeLists.txt, a different directory scope -- so the property
    # must target THAT scope via TARGET_DIRECTORY, else CMake silently
    # ignores it and the glue keeps hardware FP0.
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_dec.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_dec-ffmpeg.c
        TARGET_DIRECTORY modules_sof
        PROPERTIES COMPILE_OPTIONS "-Xclang;-target-feature;-Xclang;-fp")
    # NOTE: ffmpeg_dec-shims.c / ffmpeg_dec-alloc.c are LLEXT-only (see
    # llext/CMakeLists.txt). They provide a full private libc (malloc, str*,
    # snprintf, ...) the isolated LLEXT cannot import; in a built-in (=y) image
    # Zephyr's common libc already provides those (whole-archived), so including
    # them here would multiply-define malloc/str*/snprintf. Instead
    # ffmpeg_dec-builtin-libc.c supplies ONLY the handful of libc symbols common
    # libc does NOT export that libav* still references (stderr/fread/getenv/
    # sscanf/..., z_errno_wrap, and the __errno bridge the newlib math objects
    # linked below use on error paths) plus a no-op ffmpeg_dec_libc_bind (=y
    # allocates from common-libc malloc, no module-heap binding needed). Double
    # libm itself is real newlib math, not a stub -- see FFMPEG_TARGET_LIBM below.
    add_local_sources(sof ffmpeg_dec-builtin-libc.c)

    # =y heap routing: --wrap malloc/free/realloc/calloc (below) onto the SOF DSP
    # heap so FFmpeg av_malloc() draws from the NOBITS .heap_mem instead of the
    # image_size-capped common-libc arena (AAC-LC needs a single ~541 KiB alloc
    # that the arena cannot hold). Also provides ffmpeg_dec_libc_bind().
    add_local_sources(sof ffmpeg_dec-builtin-alloc.c)

    # Encode mode (PCM -> MP3 via libshine): add the encode backend.
    if(CONFIG_FFMPEG_DEC_ENCODE_MODE)
      add_local_sources(sof ffmpeg_dec-encode.c)
    endif()


    # Built-in (=y) real backend: cross-build the FFmpeg archive via ffmpeg.cmake
    # and statically link it into the base firmware. Mirrors the google_rtc =y
    # pattern (src/audio/google/CMakeLists.txt): headers via
    # zephyr_include_directories, archive via the SOF interface library which
    # propagates to zephyr.elf. LLD resolves all relocations at link time, so
    # this avoids the LLEXT runtime-relocation path entirely.
    is_zephyr(zephyr)
    if(zephyr)
      include(${CMAKE_CURRENT_LIST_DIR}/ffmpeg.cmake)
      zephyr_include_directories(${FFMPEG_INSTALL_DIR}/include)
      target_link_directories(SOF INTERFACE ${FFMPEG_INSTALL_DIR}/lib)
      # avcodec must precede the libs it depends on (swresample/avutil), and the
      # newlib math archive (FFMPEG_TARGET_LIBM) must come LAST so the linker
      # resolves libav*'s double libm references (sin/cos/exp/log/pow/...) against
      # it -- as an archive it only pulls the math objects actually referenced.
      # Do NOT add -lc: the base
      # image links bare-metal (-nostdlib) and the remaining libc symbols libav*
      # needs resolve from Zephyr's minimal libc already in the link (plus the
      # stubs in ffmpeg_dec-builtin-libc.c). FLAC is integer-only so it never
      # calls libm at runtime, but the references still resolve to real kernels
      # now, and lossy decoders (AAC/Opus/Vorbis) need them to decode correctly.
      target_link_libraries(SOF INTERFACE avcodec swresample avutil)
      # Encode mode: libshine MP3 encoder archive (built by ffmpeg.cmake
      # section 3b when FFMPEG_ENC_MP3 is set). Linked AFTER avcodec,
      # which references its shine_* symbols.
      if(CONFIG_FFMPEG_ENC_MP3)
        target_link_directories(SOF INTERFACE ${SHINE_INSTALL}/lib)
        target_link_libraries(SOF INTERFACE shine)
        add_dependencies(modules_sof shine_ext)
      endif()
      # libm (newlib double math) LAST so libav*/libshine references
      # (modf/cos/sin/log/sqrt/exp2) resolve against it.
      target_link_libraries(SOF INTERFACE ${FFMPEG_TARGET_LIBM})
      # Redirect libav* malloc/free/realloc/calloc to __wrap_* in
      # ffmpeg_dec-builtin-alloc.c (SOF DSP heap). --wrap resolves references to
      # the symbol -> __wrap_<sym>, and __real_<sym> -> the original common-libc
      # allocator (used pre-bind). Propagates to the zephyr.elf link via SOF.
      target_link_options(SOF INTERFACE
        -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc)
      # Both archives must be built before modules_sof links against them:
      # ffmpeg_ext (libav*) and newlib_m (libnewlib_m.a, the double math compiled
      # from vendored newlib source with -mlongcalls -- see ffmpeg.cmake 3c).
      add_dependencies(modules_sof ffmpeg_ext newlib_m)
    endif()
  endif()
endif()

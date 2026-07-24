# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_FFMPEG_DEC_STUB)
	# Dependency-free build: SOF glue + passthrough stub backend, no libavcodec.
	sof_llext_build("ffmpeg_dec"
		SOURCES ../ffmpeg_dec.c
			../ffmpeg_dec-stub.c
		LIB openmodules
	)
else()
	# Real decoder: cross-build the FFmpeg archive (Kconfig-selected decoders)
	# and link it. libc/libm are NOT bundled (non-PIC, and would clash) — the
	# module provides the needed libc symbols itself (shims/alloc), resolves the
	# rest from the SOF core, and supplies fast single-precision float math
	# (fastmathf.c) when a float codec (AAC/Opus) is selected.
	include(${CMAKE_CURRENT_LIST_DIR}/../ffmpeg.cmake)

	set(ffmpeg_dec_srcs
		../ffmpeg_dec.c
		../ffmpeg_dec-ffmpeg.c
		../ffmpeg_dec-shims.c
		../ffmpeg_dec-alloc.c)
	# fastmathf provides the single-precision libm the codecs need AND sofm_log2f
	# used by the shims' log10(); always built for the real (non-stub) backend.
	list(APPEND ffmpeg_dec_srcs ../fastmathf.c)

	# avfilter graph backend (PCM effects, e.g. afftdn noise reduction). Pulls
	# libavfilter into the link; it must precede the libs it depends on.
	set(ffmpeg_dec_libs avcodec swresample avutil)
	set(ffmpeg_dec_libpaths "${FFMPEG_INSTALL_DIR}/lib")
	if(CONFIG_FFMPEG_BUILD_AVFILTER)
		list(APPEND ffmpeg_dec_srcs ../ffmpeg_dec-filter.c)
		set(ffmpeg_dec_libs avfilter ${ffmpeg_dec_libs})
	endif()

	# Encode mode: PCM->compressed backend + libshine (linked AFTER libavcodec,
	# which references it), from its own cross-build install dir.
	if(CONFIG_FFMPEG_DEC_ENCODE_MODE)
		list(APPEND ffmpeg_dec_srcs ../ffmpeg_dec-encode.c)
	endif()
	if(CONFIG_FFMPEG_ENC_MP3)
		list(APPEND ffmpeg_dec_libs shine)
		list(APPEND ffmpeg_dec_libpaths "${SHINE_INSTALL}/lib")
	endif()

	# Cold split: module code must also use -mlongcalls to reach FFmpeg code
	# that now lives in the far (DRAM) .cold region.
	set(ffmpeg_dec_cflags "")
	if(CONFIG_FFMPEG_DEC_COLD_SPLIT)
		set(ffmpeg_dec_cflags -mlongcalls)
	endif()

	sof_llext_build("ffmpeg_dec"
		SOURCES ${ffmpeg_dec_srcs}
		INCLUDES  "${FFMPEG_INSTALL_DIR}/include"
		LIBS_PATH ${ffmpeg_dec_libpaths}
		LIBS ${ffmpeg_dec_libs}
		CFLAGS ${ffmpeg_dec_cflags}
		LIB openmodules
	)

	# The archive must be built before the module compiles/links against it.
	add_dependencies(ffmpeg_dec_llext_lib ffmpeg_ext)
	add_dependencies(ffmpeg_dec ffmpeg_ext)

	# Cold split: place the (renamed) FFmpeg .cold code so the loader relocates
	# it to DRAM. See ffmpeg_cold.ld for why INSERT AFTER .bss.
	if(CONFIG_FFMPEG_DEC_COLD_SPLIT)
		llext_link_options(ffmpeg_dec
			"-Wl,-T,${CMAKE_CURRENT_LIST_DIR}/ffmpeg_cold.ld")
	endif()
endif()

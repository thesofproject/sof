# SPDX-License-Identifier: BSD-3-Clause
#
# Cross-build decoder-only FFmpeg static libraries for the ffmpeg_dec module.
#
# Invokes FFmpeg's autotools build as an ExternalProject, cross-compiled with the
# Zephyr target toolchain, enabling only the decoders selected in Kconfig. The
# source comes from the west-pinned FFmpeg project (see west.yml). Produces
# libavcodec/libavutil/libswresample .a under ${FFMPEG_INSTALL_DIR} for the LLEXT
# to link, and defines the 'ffmpeg_ext' target it must depend on.

include(ExternalProject)

# --- 1. Locate the FFmpeg source (west module), overridable for local trees ---
if(NOT DEFINED SOF_FFMPEG_SRC_DIR)
	set(SOF_FFMPEG_SRC_DIR "${sof_top_dir}/../modules/audio/ffmpeg"
	    CACHE PATH "FFmpeg source tree (west-pinned)")
endif()
cmake_path(NORMAL_PATH SOF_FFMPEG_SRC_DIR)
if(NOT EXISTS "${SOF_FFMPEG_SRC_DIR}/configure")
	message(FATAL_ERROR
		"ffmpeg_dec: FFmpeg source not found at '${SOF_FFMPEG_SRC_DIR}'.\n"
		"Run 'west update' (FFmpeg is pinned in west.yml) or pass "
		"-DSOF_FFMPEG_SRC_DIR=<path-to-clean-ffmpeg-checkout>.")
endif()

# --- 2. Kconfig -> --enable-decoder / --enable-parser lists ---
set(_ff_decoders "")
set(_ff_parsers "")
if(CONFIG_FFMPEG_DEC_FLAC)
	list(APPEND _ff_decoders flac)
	list(APPEND _ff_parsers  flac)
endif()
if(CONFIG_FFMPEG_DEC_AAC)
	list(APPEND _ff_decoders aac aac_latm)
	list(APPEND _ff_parsers  aac aac_latm)
endif()
if(CONFIG_FFMPEG_DEC_OPUS)
	list(APPEND _ff_decoders opus)
	list(APPEND _ff_parsers  opus)
endif()
if(CONFIG_FFMPEG_DEC_MP3)
	list(APPEND _ff_decoders mp3)
	list(APPEND _ff_parsers  mpegaudio)
endif()
# A decoder is required unless this is an encoder-only build.
if(NOT _ff_decoders AND NOT CONFIG_FFMPEG_ENC_MP3)
	message(FATAL_ERROR "ffmpeg_dec: no decoder or encoder selected (Kconfig FFMPEG_DEC_*/FFMPEG_ENC_*)")
endif()
set(_ff_dec_cfg "")
if(_ff_decoders)
	list(JOIN _ff_decoders "," _ff_dec_csv)
	list(JOIN _ff_parsers  "," _ff_par_csv)
	set(_ff_dec_cfg --enable-decoder=${_ff_dec_csv} --enable-parser=${_ff_par_csv})
endif()

# --- 2b. Kconfig -> libavfilter audio filters (off unless a filter is chosen) ---
set(_ff_avfilter_cfg --disable-avfilter)
if(CONFIG_FFMPEG_BUILD_AVFILTER)
	set(_ff_filters "")
	if(CONFIG_FFMPEG_FILTER_AFFTDN)
		list(APPEND _ff_filters afftdn)
	endif()
	list(JOIN _ff_filters "," _ff_filt_csv)
	# abuffer/abuffersink feed/drain a filter graph; aformat negotiates format.
	set(_ff_avfilter_cfg
		--enable-avfilter
		--enable-filter=abuffer,abuffersink,aformat,${_ff_filt_csv})
	message(STATUS "ffmpeg_dec: enabling avfilter, filters [${_ff_filt_csv}]")
endif()

# --- 3. Derive the cross toolchain + per-compiler cross flags ---
# GCC 14 (Zephyr SDK) promotes these to errors; FFmpeg 7.x trips them.
set(_ff_extra_cflags "-fPIC -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration")

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
	# LLVM/xt-clang build. clang is a generic driver, so unlike the target-
	# specific GCC it needs the target/core/sysroot spelled out, it uses the
	# Zephyr-SDK GNU binutils for ar/as/nm/objcopy, and it cannot link the bare-
	# metal configure *test* executables itself (no default crt/linker script) --
	# so those are linked with the GNU gcc via --ld. We only ever build .a
	# archives (ar), so the linker choice affects configure tests only.
	# The GNU binutils cross-prefix normally comes from CMAKE_AR, but the Zephyr
	# LLVM toolchain (cmake/toolchain/llvm/target.cmake, BINTOOLS=llvm) sets
	# CMAKE_AR=llvm-ar and only names the SDK GNU tools through the ld it puts in
	# CMAKE_EXE_LINKER_FLAGS (--ld-path=.../xtensa-<core>_zephyr-elf-ld). Prefer
	# CMAKE_AR when it is already the SDK's xtensa ar (xt-clang), else recover the
	# SDK cross-prefix from that --ld-path. e.g. .../bin/xtensa-intel_ace30_ptl_zephyr-elf-ar:
	get_filename_component(_ff_ar_name "${CMAKE_AR}" NAME)
	if(_ff_ar_name MATCHES "^xtensa-.+_zephyr-elf-ar$")
		set(_ff_ar "${CMAKE_AR}")
	elseif(CMAKE_EXE_LINKER_FLAGS MATCHES "--ld-path=([^ ]+xtensa-[^ /]+_zephyr-elf)-ld")
		set(_ff_ar "${CMAKE_MATCH_1}-ar")
	else()
		message(FATAL_ERROR
			"ffmpeg_dec: cannot locate the Zephyr SDK xtensa binutils for the "
			"FFmpeg cross-build (CMAKE_AR='${CMAKE_AR}' is not an SDK ar and no "
			"--ld-path in CMAKE_EXE_LINKER_FLAGS). FFmpeg's configure needs the "
			"SDK sysroot/linker; clang alone has no bare-metal libc/crt.")
	endif()
	string(REGEX REPLACE "ar$" "" _ff_cross_prefix "${_ff_ar}")     # ...-elf-
	get_filename_component(_tc_dir  "${_ff_ar}" DIRECTORY)          # .../bin
	get_filename_component(_ff_gnuroot "${_tc_dir}" DIRECTORY)      # gnu root
	get_filename_component(_ff_triple  "${_ff_cross_prefix}" NAME)  # ...-elf-
	string(REGEX REPLACE "-$" "" _ff_triple "${_ff_triple}")        # triple
	set(_ff_sysroot "${_ff_gnuroot}/${_ff_triple}")
	# SDK toolchain core: triple minus the xtensa- prefix and _zephyr-elf suffix
	# (xtensa-intel_ace30_ptl_zephyr-elf -> intel_ace30_ptl).
	string(REGEX REPLACE "^xtensa-" "" _ff_core "${_ff_triple}")
	string(REGEX REPLACE "_zephyr-elf$" "" _ff_core "${_ff_core}")
	# clang's -mcpu is the upstream LLVM Xtensa core id, which for some Intel ADSP
	# cores differs from the SDK toolchain target (intel_ace30_ptl -> intel_ace30_adsp).
	# Mirror the mapping in cmake/toolchain/llvm/target.cmake (env override first,
	# then the few names that differ, else identity) so the FFmpeg objects target
	# the exact same core as the rest of the image.
	set(_ff_mcpu "$ENV{XTENSA_CORE_ID}")
	if(NOT _ff_mcpu)
		if(_ff_core STREQUAL "intel_ace30_ptl")
			set(_ff_mcpu "intel_ace30_adsp")
		elseif(_ff_core STREQUAL "intel_ace40")
			set(_ff_mcpu "intel_ace40_adsp")
		elseif(_ff_core STREQUAL "intel_ace15_mtpm")
			set(_ff_mcpu "intel_ace15_adsp")
		else()
			set(_ff_mcpu "${_ff_core}")
		endif()
	endif()
	set(_ff_cc "${CMAKE_C_COMPILER} --target=xtensa -mcpu=${_ff_mcpu} --sysroot=${_ff_sysroot}")
	set(_ff_ld "${_ff_cross_prefix}gcc")
	# clang --sysroot does NOT add GCC's internal fixed-include directory, and
	# that is where <xtensa/config/core-isa.h> lives (the GNU driver adds it
	# implicitly). Without it __has_include(<xtensa/config/core-isa.h>) fails,
	# XCHAL_HAVE_HIFI*_VFPU stay undefined, and libavutil/xtensa/float_dsp_init.c
	# silently falls back to scalar C -- i.e. the HiFi5 .sx2x2 float SIMD is
	# dropped from the ace40 build. The header lives under xtensa/ in GCC's fixed-
	# include dir, but that dir ALSO holds GCC's own <stdatomic.h> -- and putting
	# the whole dir on the include path (any of -I/-idirafter/-isystem) makes
	# clang's hosted <stdatomic.h> #include_next into GCC's, whose atomic_* macros
	# expand to the GNU __atomic_* builtins on _Atomic(int)* pointers that clang
	# rejects ("address argument to atomic operation must be a pointer to a
	# trivially-copyable type" -- hit by libavcodec/threadprogress.h). So expose
	# ONLY the xtensa/ subtree via a private dir (symlink) and -I that: clang finds
	# <xtensa/config/core-isa.h> for the VFPU detection but keeps using its own
	# freestanding <stdatomic.h> (__c11_atomic_*). (core-isa.h includes nothing
	# outside xtensa/, so the single symlink is sufficient.)
	execute_process(
		COMMAND ${_ff_cross_prefix}gcc -print-file-name=include
		OUTPUT_VARIABLE _ff_gcc_inc OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(_ff_gcc_inc AND IS_DIRECTORY "${_ff_gcc_inc}/xtensa/config")
		set(_ff_xinc "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-xtensa-inc")
		file(MAKE_DIRECTORY "${_ff_xinc}")
		file(REMOVE "${_ff_xinc}/xtensa")
		file(CREATE_LINK "${_ff_gcc_inc}/xtensa" "${_ff_xinc}/xtensa" SYMBOLIC)
		set(_ff_cc "${_ff_cc} -I${_ff_xinc}")
	endif()
	# The LLVM Xtensa backend cannot lower the SLP-vectorised v2i32 bswap that
	# FFmpeg byteswap code produces ("Cannot select: v2i32 = bswap"); disable
	# vectorisation to avoid it (and it buys nothing -- no packed float SIMD).
	set(_ff_extra_cflags "${_ff_extra_cflags} -fno-vectorize -fno-slp-vectorize")
	# A switch() compiles to a jump table whose base address is materialised with
	# an L32R against a literal pool. This Xtensa target has L32R-range/relocation
	# issues (see the zephyr auto-litpool revert), and a mis-relocated table base
	# sends the switch dispatch into garbage -- observed as a silent spin (no
	# exception breadcrumb) inside av_opt_set_defaults2's switch on option type.
	# Force branch-chain lowering (no jump-table base literal) instead.
	set(_ff_extra_cflags "${_ff_extra_cflags} -fno-jump-tables")
else()
	# GCC (Zephyr SDK): target-specific driver, brings its own sysroot + crt.
	# e.g. .../bin/xtensa-intel_ace30_ptl_zephyr-elf-gcc -> prefix ...-elf-
	get_filename_component(_tc_dir  "${CMAKE_C_COMPILER}" DIRECTORY)
	get_filename_component(_tc_name "${CMAKE_C_COMPILER}" NAME)
	string(REGEX REPLACE "gcc$" "" _tc_prefix_name "${_tc_name}")
	set(_ff_cross_prefix "${_tc_dir}/${_tc_prefix_name}")
	set(_ff_cc "${CMAKE_C_COMPILER}")
	set(_ff_ld "${CMAKE_C_COMPILER}")
endif()

# --- 3c. Build a private newlib math archive (vendored source, -mlongcalls) ---
# libavcodec/libavutil reference double libm (sin/cos/exp/log/pow/scalbn/...); the
# lossy decoders (AAC/Opus/Vorbis) actually call it. The base image uses
# CONFIG_MINIMAL_LIBC, which ships NO double libm, so the math must come from
# newlib.
#
# The SDK's PRE-BUILT newlib math cannot be linked as-is: the image is compiled
# -mlongcalls and linked --no-relax, but the SDK objects use direct call8 (there is
# no -mlongcalls multilib), so an out-of-range inter-object call (e.g. -> memset)
# is a link-time "dangerous relocation: call8: call target out of range". So we
# compile newlib's math FROM SOURCE with -mlongcalls -mtext-section-literals -- the
# compiler then emits callx8 (L32R+CALLX, unlimited range), which links cleanly
# under --no-relax. The sources are vendored under libm/ (newlib libm math/ +
# common/, the exact member set the SDK's libc.a selects -- see libm/README).
#
# Linked as an archive (FFMPEG_TARGET_LIBM, last on the link line) it pulls only
# the math objects a decoder actually calls; the whole set's external tail is
# compiler soft-float builtins (__adddf3/__muldf3/comparisons/conversions) + memset
# (already in the image) + __errno on the error paths (bridged to Zephyr's
# per-thread errno in ffmpeg_dec-builtin-libc.c).
set(_ff_libm_src "${CMAKE_CURRENT_LIST_DIR}/libm")
set(_ff_libm_dir "${CMAKE_CURRENT_BINARY_DIR}/newlib-libm")
set(FFMPEG_TARGET_LIBM "${_ff_libm_dir}/libnewlib_m.a"
    CACHE INTERNAL "newlib math compiled from source (-mlongcalls) for libav* double math")
# newlib_math_sources.cmake defines NEWLIB_MATH_SOURCES: the exact files to compile
# (relative to libm/). The 13 common/*.c it omits are only textually #included by
# their s_*.c wrappers -- compiling them standalone would multiply-define exp/log/
# pow/... (see libm/README).
include("${_ff_libm_src}/newlib_math_sources.cmake")
if(NOT NEWLIB_MATH_SOURCES)
	message(FATAL_ERROR
		"ffmpeg_dec: vendored newlib math manifest empty/missing "
		"('${_ff_libm_src}/newlib_math_sources.cmake').")
endif()
set(_ff_libm_abs "")
foreach(_s IN LISTS NEWLIB_MATH_SOURCES)
	list(APPEND _ff_libm_abs "${_ff_libm_src}/${_s}")
endforeach()
# Compile each source with the same core/sysroot as the rest of the image (_ff_cc)
# plus the same codegen guards (_ff_extra_cflags: -fno-vectorize/-fno-jump-tables/
# ...) and -mlongcalls -mtext-section-literals for --no-relax linkability, then
# archive. -ffreestanding: this is bare-metal math, no hosted libc assumptions.
string(REPLACE ";" " " _ff_libm_srclist "${_ff_libm_abs}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/newlib-m-build.sh"
"#!/bin/sh
set -e
CC=\"${_ff_cc}\"
OBJDIR=${_ff_libm_dir}/obj
mkdir -p \"\$OBJDIR\"
: > ${_ff_libm_dir}/objs.txt
for f in ${_ff_libm_srclist}; do
	o=\"\$OBJDIR/\$(basename \"\$f\").o\"
	\$CC ${_ff_extra_cflags} -mlongcalls -mtext-section-literals -O2 -ffreestanding \\
		-I${_ff_libm_src}/common -I${_ff_libm_src}/math -c \"\$f\" -o \"\$o\"
	echo \"\$o\" >> ${_ff_libm_dir}/objs.txt
done
rm -f ${FFMPEG_TARGET_LIBM}
${_ff_cross_prefix}ar rcs ${FFMPEG_TARGET_LIBM} \$(cat ${_ff_libm_dir}/objs.txt)
")
add_custom_command(
	OUTPUT "${FFMPEG_TARGET_LIBM}"
	COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/newlib-m-build.sh"
	DEPENDS ${_ff_libm_abs} "${_ff_libm_src}/newlib_math_sources.cmake"
	COMMENT "ffmpeg_dec: building newlib math archive (libnewlib_m.a, -mlongcalls)"
	VERBATIM)
add_custom_target(newlib_m DEPENDS "${FFMPEG_TARGET_LIBM}")

set(FFMPEG_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-install" CACHE INTERNAL "ffmpeg_dec libs")

# Hot/cold split: GCC emits av_cold (init/setup) functions into .text.unlikely.
# We rename that to SOF's .cold (post-install, below) so the LLEXT loader places
# it in DRAM. -mtext-section-literals interleaves each function's Xtensa L32R
# literals into its text section (as SOF does for LLEXT module code) so the
# renamed section carries its literals and L32R stays in range. We deliberately
# do NOT use -ffunction-sections: keeping one .text.unlikely per object lets a
# single objcopy --rename-section catch it, and lets .cold be placed by SOF's
# existing orphan .cold handling (a hand-written linker script that forces .cold
# ahead of .text overruns the SRAM budget and overlaps .rodata for big codecs).
if(CONFIG_FFMPEG_DEC_COLD_SPLIT)
	# -mlongcalls: cold code lives in DRAM (.cold, relocated by the loader) far
	# from the hot SRAM .text, so calls between them exceed call8 range; longcalls
	# emit an indirect L32R+CALLX with unlimited range. -mtext-section-literals
	# keeps each function's literals in its own text section so the rename carries
	# them. Coarse split: rename the whole FFmpeg .text (not just av_cold) into
	# .cold so essentially all of libav* executes from DRAM, freeing SRAM.
	set(_ff_extra_cflags "${_ff_extra_cflags} -mtext-section-literals -mlongcalls")
	set(_ff_objcopy "${_ff_cross_prefix}objcopy")
	set(_ff_cold_rename
		COMMAND ${_ff_objcopy} --rename-section .text=.cold
			${FFMPEG_INSTALL_DIR}/lib/libavcodec.a
		COMMAND ${_ff_objcopy} --rename-section .text=.cold
			${FFMPEG_INSTALL_DIR}/lib/libavutil.a
		COMMAND ${_ff_objcopy} --rename-section .text=.cold
			${FFMPEG_INSTALL_DIR}/lib/libswresample.a)
endif()

# --- 3b. MP3 encoder via libshine (fixed-point; FFmpeg has no native one) ---
set(_ff_cfg_env "PATH=${_tc_dir}:$ENV{PATH}")
set(_ff_shine_cfg "")
set(_ff_shine_dep "")
if(CONFIG_FFMPEG_ENC_MP3)
	set(SHINE_SRC "${sof_top_dir}/../modules/audio/shine")
	if(NOT EXISTS "${SHINE_SRC}/src/lib/layer3.h")
		message(FATAL_ERROR "ffmpeg_dec: libshine source not at '${SHINE_SRC}'; run 'west update'")
	endif()
	set(SHINE_INSTALL "${CMAKE_CURRENT_BINARY_DIR}/shine-install")
	file(MAKE_DIRECTORY "${SHINE_INSTALL}/lib/pkgconfig" "${SHINE_INSTALL}/include/shine")
	# pkg-config file for FFmpeg's require_pkg_config libshine.
	file(WRITE "${SHINE_INSTALL}/lib/pkgconfig/shine.pc"
"prefix=${SHINE_INSTALL}
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: shine
Description: Shine fixed-point MP3 encoder
Version: 3.1.1
Libs: -L\${libdir} -lshine
Cflags: -I\${includedir}
")
	# FFmpeg's -lshine link *test* pulls newlib malloc -> Zephyr runtime syms
	# (z_errno_wrap, ...) that only exist at module load. Provide dummies so the
	# configure test links; --extra-ldflags only affects tests, not the .a build.
	file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/shine_cfgstub.c"
"int z_errno_wrap(void){return 0;}
void *stdout;
int open(const char *p, int f, ...){return -1;}
void *sbrk(int i){return (void *)-1;}
")
	# libshine's lib needs no config.h; build the objects directly and archive
	# (its autotools CLI/shared link cannot work bare-metal).
	file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/shine-build.sh"
"#!/bin/sh
set -e
export PATH=${_tc_dir}:\$PATH
mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/shine-obj
for f in ${SHINE_SRC}/src/lib/*.c; do
	${CMAKE_C_COMPILER} -O2 -fPIC -mtext-section-literals -DSHINE_HAVE_BSWAP_H \\
		-I${SHINE_SRC}/src/lib -c \"\$f\" \\
		-o ${CMAKE_CURRENT_BINARY_DIR}/shine-obj/\$(basename \"\$f\").o
done
${_ff_cross_prefix}ar rcs ${SHINE_INSTALL}/lib/libshine.a ${CMAKE_CURRENT_BINARY_DIR}/shine-obj/*.o
cp ${SHINE_SRC}/src/lib/layer3.h ${SHINE_INSTALL}/include/shine/layer3.h
${CMAKE_C_COMPILER} -fPIC -c ${CMAKE_CURRENT_BINARY_DIR}/shine_cfgstub.c \\
	-o ${CMAKE_CURRENT_BINARY_DIR}/shine_cfgstub.o
")
	add_custom_command(
		OUTPUT "${SHINE_INSTALL}/lib/libshine.a" "${CMAKE_CURRENT_BINARY_DIR}/shine_cfgstub.o"
		COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/shine-build.sh"
		VERBATIM)
	add_custom_target(shine_ext DEPENDS "${SHINE_INSTALL}/lib/libshine.a")

	set(_ff_cfg_env "PATH=${_tc_dir}:$ENV{PATH}" "PKG_CONFIG_PATH=${SHINE_INSTALL}/lib/pkgconfig")
	set(_ff_shine_cfg --enable-libshine --enable-encoder=libshine --pkg-config=pkg-config
		"--extra-ldflags=${CMAKE_CURRENT_BINARY_DIR}/shine_cfgstub.o")
	set(_ff_shine_dep shine_ext)
	message(STATUS "ffmpeg_dec: enabling MP3 encoder via libshine (${SHINE_SRC})")
endif()

# --- 4. Configure + make + install (out-of-tree; source must be clean) ---
ExternalProject_Add(ffmpeg_ext
	DEPENDS ${_ff_shine_dep}
	SOURCE_DIR      "${SOF_FFMPEG_SRC_DIR}"
	BUILD_IN_SOURCE 0
	CONFIGURE_COMMAND
		${CMAKE_COMMAND} -E env ${_ff_cfg_env}
		<SOURCE_DIR>/configure
			--prefix=${FFMPEG_INSTALL_DIR}
			--enable-cross-compile --target-os=none --arch=xtensa
			--cross-prefix=${_ff_cross_prefix} "--cc=${_ff_cc}" "--ld=${_ff_ld}"
			# NOTE: do NOT pass --disable-asm. FFmpeg treats every per-arch
			# optimisation dir (incl. our C-intrinsic libavutil/xtensa/) as
			# "asm"; --disable-asm forces arch=c and drops them. There is no
			# Xtensa assembly, so leaving asm enabled is safe (HiFi kernels are
			# C intrinsics) and is required to build ff_float_dsp_init_xtensa.
			--disable-all --disable-everything --disable-autodetect
			--disable-programs --disable-doc --disable-network
			--disable-avformat --disable-avdevice ${_ff_avfilter_cfg}
			--disable-swscale --disable-postproc
			--disable-pthreads --disable-w32threads --disable-os2threads
			--disable-runtime-cpudetect --disable-debug
			--enable-avcodec --enable-avutil --enable-swresample
			${_ff_dec_cfg}
			${_ff_shine_cfg}
			--enable-small --enable-pic
			"--extra-cflags=${_ff_extra_cflags}"
	BUILD_COMMAND
		${CMAKE_COMMAND} -E env "PATH=${_tc_dir}:$ENV{PATH}" make -j 8
	INSTALL_COMMAND make install
		${_ff_cold_rename}
	BUILD_BYPRODUCTS
		${FFMPEG_INSTALL_DIR}/lib/libavcodec.a
		${FFMPEG_INSTALL_DIR}/lib/libavutil.a
		${FFMPEG_INSTALL_DIR}/lib/libswresample.a
)

message(STATUS "ffmpeg_dec: cross-building FFmpeg decoders [${_ff_dec_csv}] from ${SOF_FFMPEG_SRC_DIR}")

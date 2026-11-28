# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_FFMPEG_DEC_STUB)
	# Dependency-free build: SOF glue + passthrough stub backend, standalone LLEXT.
	sof_llext_build("ffmpeg_dec"
		SOURCES ../ffmpeg_common.c
			../ffmpeg_dec.c
			../ffmpeg_dec-flac.c
			../ffmpeg_dec-mp3.c
			../ffmpeg_dec-aac.c
			../ffmpeg_dec-opus.c
			../ffmpeg_dec-alac.c
			../ffmpeg_dec-sbc.c
			../ffmpeg_dec-g722.c
			../ffmpeg_dec-aptx.c
			../ffmpeg_dec-g726.c
			../ffmpeg_dec-g711.c
			../ffmpeg_dec-stub.c
	)
	set(FFMPEG_LLEXT_TARGETS ffmpeg_dec PARENT_SCOPE)
	return()
endif()

# Real backends: cross-build FFmpeg, libshine, vo-aacenc, and newlib_m once,
# then link each enabled decoder, encoder, and filter as its own standalone
# .llext module (no LIB openmodules packing) with per-module registration tables
# so unused codecs are never pulled into the module.
set(_ff_root_dir "${CMAKE_CURRENT_LIST_DIR}/..")
set(_ff_llext_dir "${CMAKE_CURRENT_LIST_DIR}")
include("${_ff_root_dir}/ffmpeg.cmake")

set(_ff_common_cflags "")
if(CONFIG_FFMPEG_DEC_COLD_SPLIT)
	list(APPEND _ff_common_cflags -mlongcalls)
else()
	list(APPEND _ff_common_cflags -ffunction-sections -fdata-sections -fvisibility=hidden)
endif()
# Disable scalar hardware FP0 on Clang targets (CONFIG_FPU=n) so float
# conversions and strtod/strtof emit soft-float libcalls instead of
# faulting FP0 instructions.
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
	list(APPEND _ff_common_cflags "SHELL:-Xclang -target-feature -Xclang -fp")
endif()

get_filename_component(_ff_libm_dir "${FFMPEG_TARGET_LIBM}" DIRECTORY)

set(_ff_all_targets "")

# Helper: generate per-module config header, codec/parser/filter registration
# lists + archive, llext.toml.h and CMakeLists.txt, and register sof_llext_build().
function(_ffmpeg_add_llext_module mod_name)
	set(options USE_FASTMATH USE_AVFILTER USE_SHINE USE_VOAACENC)
	set(one_val ROLE MANIFEST_NAME UUID_SYM IBS OBS CODEC_OPS SND_ID BESPOKE AF_DEFINE)
	set(multi_val SOURCES CODEC_SYMS PARSER_SYMS FILTER_SYMS EXTRA_DEPS)
	cmake_parse_arguments(ARG "${options}" "${one_val}" "${multi_val}" ${ARGN})

	set(_mod_src_dir "${CMAKE_CURRENT_BINARY_DIR}/mods/${mod_name}")
	file(MAKE_DIRECTORY "${_mod_src_dir}/libavcodec" "${_mod_src_dir}/libavfilter")

	# 1. Generate TOML header from template
	string(TOUPPER "${ARG_UUID_SYM}" _uuid_upper)
	set(FFMPEG_MOD_NAME "${mod_name}")
	set(FFMPEG_MOD_MANIFEST_NAME "${ARG_MANIFEST_NAME}")
	set(FFMPEG_MOD_UUID_MACRO "UUIDREG_STR_${_uuid_upper}")
	set(FFMPEG_MOD_IBS "${ARG_IBS}")
	set(FFMPEG_MOD_OBS "${ARG_OBS}")
	configure_file("${_ff_root_dir}/ffmpeg_mod.toml.in"
		       "${_mod_src_dir}/llext.toml.h" @ONLY)

	# 2. Generate per-module macro header (included via -imacros to avoid shell quoting)
	set(_cfg_h "#ifndef _FFMPEG_MOD_CFG_H_\n#define _FFMPEG_MOD_CFG_H_\n")
	if(ARG_ROLE STREQUAL "dec")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_ROLE_DEC 1\n")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_CODEC_OPS ${ARG_CODEC_OPS}\n")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_SND_CODEC_ID ${ARG_SND_ID}\n")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_BESPOKE ${ARG_BESPOKE}\n")
	elseif(ARG_ROLE STREQUAL "enc")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_ROLE_ENC 1\n")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_CODEC_OPS ${ARG_CODEC_OPS}\n")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_SND_CODEC_ID ${ARG_SND_ID}\n")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_BESPOKE ${ARG_BESPOKE}\n")
	elseif(ARG_ROLE STREQUAL "af")
		set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_ROLE_AF 1\n")
		set(_cfg_h "${_cfg_h}#define ${ARG_AF_DEFINE} 1\n")
	endif()
	set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_MANIFEST_NAME \"${ARG_MANIFEST_NAME}\"\n")
	set(_cfg_h "${_cfg_h}#define FFMPEG_MOD_UUID_SYM ${ARG_UUID_SYM}\n")
	set(_cfg_h "${_cfg_h}#endif\n")
	file(WRITE "${_mod_src_dir}/ffmpeg_mod_cfg.h" "${_cfg_h}")

	# 3. Generate per-module registration lists and static archive (lib<mod>_reg.a)
	set(_reg_lib "")
	set(_reg_dep "")
	set(_need_reg FALSE)
	set(_reg_cmds "")
	set(_reg_objs "")

	if(ARG_CODEC_SYMS OR ARG_PARSER_SYMS)
		set(_need_reg TRUE)
		set(_c_decls "")
		set(_c_list "")
		foreach(_cs IN LISTS ARG_CODEC_SYMS)
			set(_c_decls "${_c_decls}extern const FFCodec ${_cs};\n")
			set(_c_list "${_c_list}    &${_cs},\n")
		endforeach()
		set(_p_decls "")
		set(_p_list "")
		foreach(_ps IN LISTS ARG_PARSER_SYMS)
			set(_p_decls "${_p_decls}extern const AVCodecParser ${_ps};\n")
			set(_p_list "${_p_list}    &${_ps},\n")
		endforeach()
		file(WRITE "${_mod_src_dir}/libavcodec/codec_list.c"
			"${_c_decls}static const FFCodec * const codec_list[] = {\n${_c_list}    NULL };\n")
		file(WRITE "${_mod_src_dir}/libavcodec/parser_list.c"
			"${_p_decls}static const AVCodecParser * const parser_list[] = {\n${_p_list}    NULL };\n")
		set(_reg_cmds "${_reg_cmds}
\$CC \$CFLAGS -c \"${SOF_FFMPEG_SRC_DIR}/libavcodec/allcodecs.c\" -o \"${_mod_src_dir}/allcodecs.o\"
\$CC \$CFLAGS -c \"${SOF_FFMPEG_SRC_DIR}/libavcodec/parsers.c\" -o \"${_mod_src_dir}/parsers.o\"")
		set(_reg_objs "${_reg_objs} \"${_mod_src_dir}/allcodecs.o\" \"${_mod_src_dir}/parsers.o\"")
	endif()

	if(ARG_USE_AVFILTER)
		set(_need_reg TRUE)
		set(_f_decls "")
		set(_f_list "")
		foreach(_fs IN LISTS ARG_FILTER_SYMS)
			set(_f_decls "${_f_decls}extern const AVFilter ${_fs};\n")
			set(_f_list "${_f_list}    &${_fs},\n")
		endforeach()
		file(WRITE "${_mod_src_dir}/libavfilter/filter_list.c"
			"${_f_decls}static const AVFilter * const filter_list[] = {\n${_f_list}    NULL };\n")
		set(_reg_cmds "${_reg_cmds}
\$CC \$CFLAGS -c \"${SOF_FFMPEG_SRC_DIR}/libavfilter/allfilters.c\" -o \"${_mod_src_dir}/allfilters.o\"")
		set(_reg_objs "${_reg_objs} \"${_mod_src_dir}/allfilters.o\"")
	endif()

	if(_need_reg)
		set(_reg_archive "${_mod_src_dir}/lib${mod_name}_reg.a")
		file(WRITE "${_mod_src_dir}/build_reg.sh"
"#!/bin/sh
set -e
CC=\"${FFMPEG_CC_FF}\"
CFLAGS=\"-Os ${FFMPEG_EXTRA_CFLAGS} -DHAVE_AV_CONFIG_H -I${_mod_src_dir} -I${FFMPEG_EXT_BUILD_DIR} -I${SOF_FFMPEG_SRC_DIR}\"
${_reg_cmds}
rm -f \"${_reg_archive}\"
${FFMPEG_CROSS_PREFIX}ar rcs \"${_reg_archive}\" ${_reg_objs}
")
		add_custom_command(
			OUTPUT "${_reg_archive}"
			COMMAND sh "${_mod_src_dir}/build_reg.sh"
			DEPENDS ffmpeg_ext "${_mod_src_dir}/build_reg.sh"
			VERBATIM)
		add_custom_target(${mod_name}_reg DEPENDS "${_reg_archive}")
		set(_reg_lib "${mod_name}_reg")
		set(_reg_dep "${mod_name}_reg")
	endif()

	# 4. Build SOURCES, INCLUDES, LIBS, LIBS_PATH, CFLAGS lists
	set(_srcs
		"${_ff_root_dir}/ffmpeg_common.c"
		"${_ff_root_dir}/ffmpeg_dec-convert.c"
		"${_ff_root_dir}/ffmpeg-convert-hifi.c"
		"${_ff_root_dir}/ffmpeg_dec-shims.c"
		"${_ff_root_dir}/ffmpeg_dec-alloc.c"
		${ARG_SOURCES})
	if(ARG_USE_FASTMATH)
		list(APPEND _srcs "${_ff_root_dir}/fastmathf.c")
	endif()

	set(_incs "${FFMPEG_INSTALL_DIR}/include" "${_ff_root_dir}")
	set(_libpaths "${_mod_src_dir}" "${FFMPEG_INSTALL_DIR}/lib")
	set(_libs "")

	if(_reg_lib)
		list(APPEND _libs "${_reg_lib}")
	endif()
	if(ARG_USE_AVFILTER)
		list(APPEND _libs avfilter_noco)
	endif()
	if(ARG_CODEC_SYMS OR ARG_PARSER_SYMS)
		list(APPEND _libs avcodec_noco)
	endif()
	# Repeat _reg_lib (and avcodec_noco when parsers are present) because
	# archive members in avcodec_noco (parser.o -> av_parser_iterate)
	# pull parsers.o, which in turn references ff_*_parser in avcodec_noco.
	if(_reg_lib)
		list(APPEND _libs "${_reg_lib}")
	endif()
	if(ARG_PARSER_SYMS)
		list(APPEND _libs avcodec_noco)
	endif()
	if(ARG_USE_SHINE)
		list(APPEND _libs shine)
		list(APPEND _libpaths "${SHINE_INSTALL}/lib")
	endif()
	if(ARG_USE_VOAACENC)
		list(APPEND _libs vo-aacenc)
		list(APPEND _incs "${VO_AACENC_INSTALL}/include")
		list(APPEND _libpaths "${VO_AACENC_INSTALL}/lib")
	endif()
	if(ARG_ROLE STREQUAL "dec" OR ARG_USE_AVFILTER)
		list(APPEND _libs swresample)
	endif()
	list(APPEND _libs avutil newlib_m)
	list(APPEND _libpaths "${_ff_libm_dir}")

	set(_cflags ${_ff_common_cflags} "SHELL:-imacros ${_mod_src_dir}/ffmpeg_mod_cfg.h")
	set(_deps ffmpeg_ext newlib_m ${_reg_dep} ${ARG_EXTRA_DEPS})

	set(_srcs_str "")
	foreach(_s IN LISTS _srcs)
		set(_srcs_str "${_srcs_str}\n\t\"${_s}\"")
	endforeach()
	set(_incs_str "")
	foreach(_i IN LISTS _incs)
		set(_incs_str "${_incs_str}\n\t\"${_i}\"")
	endforeach()
	set(_libpaths_str "")
	foreach(_p IN LISTS _libpaths)
		set(_libpaths_str "${_libpaths_str}\n\t\"${_p}\"")
	endforeach()
	set(_libs_str "")
	foreach(_l IN LISTS _libs)
		set(_libs_str "${_libs_str} ${_l}")
	endforeach()
	set(_cflags_str "")
	foreach(_c IN LISTS _cflags)
		set(_cflags_str "${_cflags_str} \"${_c}\"")
	endforeach()
	set(_deps_str "")
	foreach(_d IN LISTS _deps)
		set(_deps_str "${_deps_str} ${_d}")
	endforeach()

	set(_link_opts_list "")
	if(NOT CONFIG_LLEXT_TYPE_ELF_RELOCATABLE)
		list(APPEND _link_opts_list "\"-Wl,--exclude-libs,ALL\"" "\"-Wl,--gc-sections\"")
	endif()
	if(CONFIG_FFMPEG_DEC_COLD_SPLIT)
		list(APPEND _link_opts_list "\"-Wl,-T,${_ff_llext_dir}/ffmpeg_cold.ld\"")
	endif()
	set(_link_opts_str "")
	if(_link_opts_list)
		string(JOIN " " _link_opts_joined ${_link_opts_list})
		set(_link_opts_str "\n\tLINK_OPTIONS ${_link_opts_joined}")
	endif()

	file(WRITE "${_mod_src_dir}/CMakeLists.txt"
"# Auto-generated per-module LLEXT build for ${mod_name}
sof_llext_build(\"${mod_name}\"
	SOURCES ${_srcs_str}
	INCLUDES ${_incs_str}
	LIBS_PATH ${_libpaths_str}
	LIBS ${_libs_str}
	CFLAGS ${_cflags_str}${_link_opts_str}
)
add_dependencies(${mod_name}_llext_lib ${_deps_str})
add_dependencies(${mod_name} ${_deps_str})
")

	add_subdirectory("${_mod_src_dir}" "${PROJECT_BINARY_DIR}/${mod_name}_llext")
	list(APPEND _ff_all_targets "${mod_name}")
	set(_ff_all_targets "${_ff_all_targets}" PARENT_SCOPE)
endfunction()

# --- Decoders (IBS=4096, OBS=16384) ---
macro(_ffmpeg_add_decoder kconf mod_name manifest_name src_file ops_sym snd_id bespoke ff_dec_syms ff_par_syms use_fastmath)
	if(${kconf})
		set(_fm_flag "")
		if(${use_fastmath})
			set(_fm_flag USE_FASTMATH)
		endif()
		_ffmpeg_add_llext_module("${mod_name}"
			ROLE dec
			MANIFEST_NAME "${manifest_name}"
			UUID_SYM "${mod_name}"
			IBS 4096 OBS 16384
			CODEC_OPS "${ops_sym}"
			SND_ID "${snd_id}"
			BESPOKE "${bespoke}"
			SOURCES "${_ff_root_dir}/ffmpeg_dec.c"
				"${_ff_root_dir}/ffmpeg_dec-ffmpeg.c"
				"${_ff_root_dir}/${src_file}"
			CODEC_SYMS ${ff_dec_syms}
			PARSER_SYMS ${ff_par_syms}
			${_fm_flag}
		)
	endif()
endmacro()

_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_FLAC    ffmpeg_dec_flac    FFDECFLC ffmpeg_dec-flac.c ffmpeg_dec_flac_ops    SND_AUDIOCODEC_FLAC     0 "ff_flac_decoder"                              "ff_flac_parser"                     FALSE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_AAC     ffmpeg_dec_aac     FFDECAAC ffmpeg_dec-aac.c  ffmpeg_dec_aac_ops     SND_AUDIOCODEC_AAC      0 "ff_aac_fixed_decoder"                         "ff_aac_parser;ff_aac_latm_parser"   TRUE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_OPUS    ffmpeg_dec_opus    FFDECOPS ffmpeg_dec-opus.c ffmpeg_dec_opus_ops    SND_AUDIOCODEC_OPUS_RAW 1 "ff_opus_decoder"                              "ff_opus_parser"                     TRUE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_MP3     ffmpeg_dec_mp3     FFDECMP3 ffmpeg_dec-mp3.c  ffmpeg_dec_mp3_ops     SND_AUDIOCODEC_MP3      0 "ff_mp3_decoder"                               "ff_mpegaudio_parser"                TRUE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_ALAC    ffmpeg_dec_alac    FFDECALC ffmpeg_dec-alac.c ffmpeg_dec_alac_ops    SND_AUDIOCODEC_ALAC     0 "ff_alac_decoder"                              ""                                   FALSE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_SBC     ffmpeg_dec_sbc     FFDECSBC ffmpeg_dec-sbc.c  ffmpeg_dec_sbc_ops     SND_AUDIOCODEC_SBC      1 "ff_sbc_decoder"                               "ff_sbc_parser"                      FALSE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_G722    ffmpeg_dec_g722    FFDEC722 ffmpeg_dec-g722.c ffmpeg_dec_g722_ops    SND_AUDIOCODEC_G722     1 "ff_adpcm_g722_decoder"                        ""                                   FALSE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_APTX    ffmpeg_dec_aptx    FFDECAPX ffmpeg_dec-aptx.c ffmpeg_dec_aptx_ops    SND_AUDIOCODEC_APTX     1 "ff_aptx_decoder"                              ""                                   FALSE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_APTX_HD ffmpeg_dec_aptx_hd FFDECAHD ffmpeg_dec-aptx.c ffmpeg_dec_aptx_hd_ops SND_AUDIOCODEC_APTX_HD  1 "ff_aptx_hd_decoder"                           ""                                   FALSE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_G726    ffmpeg_dec_g726    FFDEC726 ffmpeg_dec-g726.c ffmpeg_dec_g726_ops    SND_AUDIOCODEC_G726     1 "ff_adpcm_g726_decoder;ff_adpcm_g726le_decoder" ""                                  FALSE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_ALAW    ffmpeg_dec_alaw    FFDECALW ffmpeg_dec-g711.c ffmpeg_dec_alaw_ops    SND_AUDIOCODEC_ALAW     1 "ff_pcm_alaw_decoder"                          ""                                   FALSE)
_ffmpeg_add_decoder(CONFIG_FFMPEG_DEC_MULAW   ffmpeg_dec_mulaw   FFDECULW ffmpeg_dec-g711.c ffmpeg_dec_mulaw_ops   SND_AUDIOCODEC_MULAW    1 "ff_pcm_mulaw_decoder"                         ""                                   FALSE)

# --- Encoders (IBS=16384, OBS=4096) ---
macro(_ffmpeg_add_encoder kconf mod_name manifest_name src_file ops_sym snd_id bespoke ff_enc_syms)
	if(${kconf})
		_ffmpeg_add_llext_module("${mod_name}"
			ROLE enc
			MANIFEST_NAME "${manifest_name}"
			UUID_SYM "${mod_name}"
			IBS 16384 OBS 4096
			CODEC_OPS "${ops_sym}"
			SND_ID "${snd_id}"
			BESPOKE "${bespoke}"
			SOURCES "${_ff_root_dir}/ffmpeg-encode.c"
				"${_ff_root_dir}/${src_file}"
			CODEC_SYMS ${ff_enc_syms}
			${ARGN}
		)
	endif()
endmacro()

_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_MP3     ffmpeg_enc_mp3     FFENCMP3 ffmpeg-enc-mp3.c  ffmpeg_enc_mp3_ops     SND_AUDIOCODEC_MP3     0 "ff_libshine_encoder"                         USE_SHINE EXTRA_DEPS shine_ext)
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_AAC     ffmpeg_enc_aac     FFENCAAC ffmpeg-enc-aac.c  ffmpeg_enc_aac_ops     SND_AUDIOCODEC_AAC     0 ""                                            USE_VOAACENC EXTRA_DEPS vo_aacenc_ext)
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_FLAC    ffmpeg_enc_flac    FFENCFLC ffmpeg-enc-flac.c ffmpeg_enc_flac_ops    SND_AUDIOCODEC_FLAC    0 "ff_flac_encoder")
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_ALAC    ffmpeg_enc_alac    FFENCALC ffmpeg-enc-alac.c ffmpeg_enc_alac_ops    SND_AUDIOCODEC_ALAC    0 "ff_alac_encoder")
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_SBC     ffmpeg_enc_sbc     FFENCSBC ffmpeg-enc-sbc.c  ffmpeg_enc_sbc_ops     SND_AUDIOCODEC_SBC     1 "ff_sbc_encoder")
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_G722    ffmpeg_enc_g722    FFENC722 ffmpeg-enc-g722.c ffmpeg_enc_g722_ops    SND_AUDIOCODEC_G722    1 "ff_adpcm_g722_encoder")
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_APTX    ffmpeg_enc_aptx    FFENCAPX ffmpeg-enc-aptx.c ffmpeg_enc_aptx_ops    SND_AUDIOCODEC_APTX    1 "ff_aptx_encoder")
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_APTX_HD ffmpeg_enc_aptx_hd FFENCAHD ffmpeg-enc-aptx.c ffmpeg_enc_aptx_hd_ops SND_AUDIOCODEC_APTX_HD 1 "ff_aptx_hd_encoder")
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_G726    ffmpeg_enc_g726    FFENC726 ffmpeg-enc-g726.c ffmpeg_enc_g726_ops    SND_AUDIOCODEC_G726    1 "ff_adpcm_g726_encoder;ff_adpcm_g726le_encoder")
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_ALAW    ffmpeg_enc_alaw    FFENCALW ffmpeg-enc-g711.c ffmpeg_enc_alaw_ops    SND_AUDIOCODEC_ALAW    1 "ff_pcm_alaw_encoder")
_ffmpeg_add_encoder(CONFIG_FFMPEG_ENC_MULAW   ffmpeg_enc_mulaw   FFENCULW ffmpeg-enc-g711.c ffmpeg_enc_mulaw_ops   SND_AUDIOCODEC_MULAW   1 "ff_pcm_mulaw_encoder")

# --- Audio Filters (IBS=16384, OBS=16384) ---
if(CONFIG_FFMPEG_FILTER_AFFTDN)
	_ffmpeg_add_llext_module(ffmpeg_af_afftdn
		ROLE af
		MANIFEST_NAME "FFAFDN"
		UUID_SYM ffmpeg_af_afftdn
		IBS 16384 OBS 16384
		AF_DEFINE FFMPEG_AF_SINGLE_AFFTDN
		SOURCES "${_ff_root_dir}/ffmpeg-filter.c"
		FILTER_SYMS ff_asrc_abuffer ff_asink_abuffer ff_af_aformat ff_af_afftdn
		USE_AVFILTER
		USE_FASTMATH
	)
endif()
if(CONFIG_FFMPEG_FILTER_ALIMITER)
	_ffmpeg_add_llext_module(ffmpeg_af_alimiter
		ROLE af
		MANIFEST_NAME "FFALIM"
		UUID_SYM ffmpeg_af_alimiter
		IBS 16384 OBS 16384
		AF_DEFINE FFMPEG_AF_SINGLE_ALIMITER
		SOURCES "${_ff_root_dir}/ffmpeg-filter.c"
		FILTER_SYMS ff_asrc_abuffer ff_asink_abuffer ff_af_aformat ff_af_alimiter
		USE_AVFILTER
		USE_FASTMATH
	)
endif()

set(FFMPEG_LLEXT_TARGETS ${_ff_all_targets} PARENT_SCOPE)

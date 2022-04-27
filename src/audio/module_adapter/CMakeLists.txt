# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof module_adapter.c module/generic.c)

if(CONFIG_COMP_VOLUME)
add_local_sources(sof module/volume/volume_generic.c module/volume/volume_hifi3.c module/volume/volume.c)
endif()

if(CONFIG_CADENCE_CODEC)
add_local_sources(sof module/cadence.c)

if(CONFIG_CADENCE_CODEC_WRAPPER)
sof_add_static_library(codec_wrapper_lib ${CONFIG_CADENCE_CODEC_WRAPPER_LIB})
endif()

if(CONFIG_CADENCE_CODEC_AAC_DEC)
sof_add_static_library(xa_aac_dec ${CONFIG_CADENCE_CODEC_AAC_DEC_LIB})
endif()

if(CONFIG_CADENCE_CODEC_BSAC_DEC)
sof_add_static_library(xa_bsac_dec ${CONFIG_CADENCE_CODEC_BSAC_DEC_LIB})
endif()


if(CONFIG_CADENCE_CODEC_DAB_DEC)
sof_add_static_library(xa_dab_dec ${CONFIG_CADENCE_CODEC_DAB_DEC_LIB})
endif()


if(CONFIG_CADENCE_CODEC_DRM_DEC)
sof_add_static_library(xa_drm_dec ${CONFIG_CADENCE_CODEC_DRM_DEC_LIB})
endif()


if(CONFIG_CADENCE_CODEC_MP3_DEC)
sof_add_static_library(xa_mp3_dec ${CONFIG_CADENCE_CODEC_MP3_DEC_LIB})
endif()


if(CONFIG_CADENCE_CODEC_SBC_DEC)
sof_add_static_library(xa_sbc_dec ${CONFIG_CADENCE_CODEC_SBC_DEC_LIB})
endif()

if(CONFIG_CADENCE_CODEC_VORBIS_DEC)
	sof_add_static_library(xa_vorbis_dec ${CONFIG_CADENCE_CODEC_VORBIS_DEC_LIB})
endif()

if(CONFIG_CADENCE_CODEC_SRC_PP)
	sof_add_static_library(xa_src_pp ${CONFIG_CADENCE_CODEC_SRC_PP_LIB})
endif()

endif()

if(CONFIG_PASSTHROUGH_CODEC)
add_local_sources(sof module/passthrough.c)
endif()

if(CONFIG_WAVES_CODEC)
add_local_sources(sof module/waves.c)
sof_add_static_library(MaxxChrome ${CMAKE_CURRENT_LIST_DIR}/lib/release/libMaxxChrome.a)
# folder with Waves API must be among include directories
target_include_directories(sof PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../include/sof/audio/)
endif()

if(CONFIG_DTS_CODEC)
add_local_sources(sof module/dts.c)
target_compile_definitions(sof PRIVATE -DDTS_MATH_INT32 -DDTS_XTENSA)
target_include_directories(sof PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include/sof/audio/dts/)
sof_add_static_library(DtsCodec ${CMAKE_CURRENT_LIST_DIR}/lib/release/libdts-sof-interface-i32.a)
endif()

# Audio Modules with various optimizaitons. Duplicated for now. Should be removed when all modules
# move to using the module adapter.

# add rules for module compilation and installation
function(sof_audio_add_module lib_name compile_flags)
	add_library(${lib_name} MODULE "")
	target_link_libraries(${lib_name} PRIVATE sof_options)
	target_link_libraries(${lib_name} PRIVATE -Wl,--export-dynamic)
	target_compile_options(${lib_name} PRIVATE ${compile_flags})
	add_local_sources(${lib_name} ${ARGN})
	sof_append_relative_path_definitions(${lib_name})
	install(TARGETS ${lib_name} DESTINATION lib)
endfunction()

# add rules for module compilation and installation
function(sof_audio_add_module lib_name compile_flags)
	add_library(${lib_name} MODULE "")
	target_link_libraries(${lib_name} PRIVATE sof_options)
	target_link_libraries(${lib_name} PRIVATE -Wl,--export-dynamic)
	target_compile_options(${lib_name} PRIVATE ${compile_flags})
	add_local_sources(${lib_name} ${ARGN})
	sof_append_relative_path_definitions(${lib_name})
	install(TARGETS ${lib_name} DESTINATION lib)
endfunction()

include(CheckCCompilerFlag)

set(available_optimizations)

# checks if flag is supported by compiler and sets needed flags
# note: to debug vectorisation please add "-fopt-info-vec-note" option after
# the enable command below.
macro(check_optimization opt_name flag enable_cmd extra_define)
	check_c_compiler_flag(${flag} compiles_flag_${opt_name})
	if(compiles_flag_${opt_name})
		list(APPEND available_optimizations ${opt_name})
		set(${opt_name}_flags ${flag} ${extra_define} ${enable_cmd} -ffast-math)
	endif()
endmacro()

# modules will be compiled only for flags supported by compiler
check_optimization(sse42 -msse4.2 -ftree-vectorize -DOPS_SSE42)
check_optimization(avx -mavx -ftree-vectorize -DOPS_AVX)
check_optimization(avx2 -mavx2 -ftree-vectorize -DOPS_AVX2)
check_optimization(fma -mfma -ftree-vectorize -DOPS_FMA)
check_optimization(hifi2ep -mhifi2ep "" -DOPS_HIFI2EP)
check_optimization(hifi3 -mhifi3 "" -DOPS_HIFI3)

set(sof_audio_modules volume)

# sources for each module
set(volume_sources module/volume/volume.c module/volume/volume_generic.c)

foreach(audio_module ${sof_audio_modules})
	# first compile with no optimizations
	sof_audio_add_module(sof_${audio_module} "" ${${audio_module}_sources})

	# compile for each optimization
	foreach(opt ${available_optimizations})
		sof_audio_add_module(sof_${audio_module}_${opt} "${${opt}_flags}" ${${audio_module}_sources})
	endforeach()
endforeach()

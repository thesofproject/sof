# SPDX-License-Identifier: BSD-3-Clause

if(NOT CONFIG_LIBRARY)
	if (CONFIG_IPC_MAJOR_4 OR CONFIG_CADENCE_CODEC OR (NOT CONFIG_COMP_LEGACY_INTERFACE))
	add_local_sources(sof module_adapter.c module/generic.c)
	endif()

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

	return()
endif()

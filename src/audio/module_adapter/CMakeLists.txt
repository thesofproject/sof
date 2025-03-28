# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_IPC_MAJOR_3)
	add_local_sources(sof module_adapter.c module_adapter_ipc3.c module/generic.c)
elseif(CONFIG_IPC_MAJOR_4)
	add_local_sources(sof module_adapter.c module_adapter_ipc4.c module/generic.c)
endif()

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

	# modules and codecs in alphabetical order

	zephyr_library_sources_ifdef(CONFIG_CADENCE_CODEC module/cadence.c)

	if (CONFIG_CADENCE_CODEC_AAC_DEC)
		zephyr_library_import(xa_aac_dec ${CONFIG_CADENCE_CODEC_AAC_DEC_LIB})
	endif()

	if (CONFIG_CADENCE_CODEC_MP3_DEC)
		zephyr_library_import(xa_mp3_dec ${CONFIG_CADENCE_CODEC_MP3_DEC_LIB})
	endif()

	if (CONFIG_CADENCE_CODEC_MP3_ENC)
		zephyr_library_import(xa_mp3_enc ${CONFIG_CADENCE_CODEC_MP3_ENC_LIB})
	endif()

	zephyr_include_directories_ifdef(CONFIG_INTEL_MODULES
		${SOF_SRC_PATH}/include/sof/audio/module_adapter/iadk/
		${SOF_SRC_PATH}/include/sof/audio/module_adapter/library/
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_MODULES
		module/modules.c
		iadk/module_initial_settings_concrete.cpp
		iadk/iadk_module_adapter.cpp
		iadk/system_agent.cpp
		library/native_system_agent.c
		library/native_system_service.c
	)

	zephyr_library_sources_ifdef(CONFIG_PASSTHROUGH_CODEC
		module/passthrough.c
	)

	if(CONFIG_WAVES_CODEC)
		set(WAVES_DIR module/waves)
		if(CONFIG_WAVES_CODEC STREQUAL "m")
			add_subdirectory(${WAVES_DIR}/llext ${PROJECT_BINARY_DIR}/waves_llext)
			add_dependencies(app waves)
		else()
			zephyr_library_sources(${WAVES_DIR}/waves.c)
			if(CONFIG_WAVES_CODEC_STUB)
				zephyr_library_sources_ifdef(CONFIG_WAVES_CODEC_STUB
							     ${WAVES_DIR}/maxx_stub.c)
			else()
				zephyr_library_import(waves_codec
					${sof_top_dir}/third_party/lib/libMaxxChrome.a)
			endif()
		endif()
	endif()

	return()
endif()

if(NOT CONFIG_COMP_MODULE_SHARED_LIBRARY_BUILD)
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

		if(CONFIG_CADENCE_CODEC_MP3_ENC)
			sof_add_static_library(xa_mp3_enc ${CONFIG_CADENCE_CODEC_MP3_ENC_LIB})
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
		add_local_sources(sof module/waves/waves.c)
		if (CONFIG_WAVES_CODEC_STUB)
			add_local_sources(sof module/waves/maxx_stub.c)
		else()
			sof_add_static_library(MaxxChrome ${CMAKE_CURRENT_LIST_DIR}/lib/release/libMaxxChrome.a)
		endif()
	# folder with Waves API must be among include directories
	endif()

	return()
endif()

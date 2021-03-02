# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof codec_adapter.c codec/generic.c)

if(CONFIG_CADENCE_CODEC)
add_local_sources(sof codec/cadence.c)

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

endif()

if(CONFIG_PASSTHROUGH_CODEC)
add_local_sources(sof codec/passthrough.c)
endif()

if(CONFIG_WAVES_CODEC)
add_local_sources(sof codec/waves.c)
sof_add_static_library(MaxxChrome ${CMAKE_CURRENT_LIST_DIR}/lib/release/libMaxxChrome.a)
# folder with Waves API must be among include directories
target_include_directories(sof PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../include/sof/audio/)
endif()

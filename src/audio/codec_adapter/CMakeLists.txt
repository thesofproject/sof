# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof codec_adapter.c codec/generic.c)

if(CONFIG_CADENCE_CODEC)
add_local_sources(sof codec/cadence.c)
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

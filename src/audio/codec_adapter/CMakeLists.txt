# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof codec_adapter.c codec/generic.c)

if(CONFIG_CADENCE_CODEC)
add_local_sources(sof codec/cadence.c)
endif()

if(CONFIG_DUMMY_CODEC)
add_local_sources(sof codec/dummy.c)
endif()

if(CONFIG_WAVES_CODEC)
add_local_sources(sof codec/waves.c)
sof_add_static_library(MaxxChrome ${CMAKE_CURRENT_LIST_DIR}/lib/release/libMaxxChrome.a)
endif()

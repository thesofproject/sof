# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof codec_adapter.c codec/generic.c)
if(CONFIG_CADENCE_CODEC)
add_local_sources(sof codec/cadence.c)
endif()
if(CONFIG_DCBLOCK_CODEC)
add_local_sources(sof codec/dcblock.c)
endif()

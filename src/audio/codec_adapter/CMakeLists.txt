# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof codec_adapter.c codec/generic.c)
if(CONFIG_CADENCE_CODEC)
add_local_sources(sof codec/cadence.c)
endif()

if(CONFIG_DUMMY_CODEC)
add_local_sources(sof codec/dummy.c)
endif()

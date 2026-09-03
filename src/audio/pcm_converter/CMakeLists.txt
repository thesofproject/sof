# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	pcm_converter.c
	pcm_converter_generic.c
	pcm_converter_hifi3.c)

if(CONFIG_PCM_REMAPPING_CONVERTERS)
	add_local_sources(sof pcm_remap.c)
endif()

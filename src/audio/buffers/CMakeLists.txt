# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof audio_buffer.c)
add_local_sources(sof comp_buffer.c)

if(CONFIG_PIPELINE_2_0)
	add_local_sources(sof ring_buffer.c)
endif()

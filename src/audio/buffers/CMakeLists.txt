# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof audio_buffer.c comp_buffer.c)

add_local_sources_ifdef(CONFIG_PIPELINE_2_0 sof ring_buffer.c)

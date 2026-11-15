# SPDX-License-Identifier: BSD-3-Clause

add_local_sources_ifdef(CONFIG_SOF_DEBUG_STREAM_SLOT sof debug_stream_slot.c)

add_local_sources_ifdef(CONFIG_SOF_DEBUG_STREAM_THREAD_INFO sof debug_stream_thread_info.c)

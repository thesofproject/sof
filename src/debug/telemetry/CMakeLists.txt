# SPDX-License-Identifier: BSD-3-Clause

add_local_sources_ifdef(CONFIG_SOF_TELEMETRY sof telemetry.c)

add_local_sources_ifdef(CONFIG_SOF_TELEMETRY2_SLOT sof telemetry2_slot.c)

add_local_sources_ifdef(CONFIG_SOF_TELEMETRY2_THREAD_INFO sof telemetry2_thread_info.c)

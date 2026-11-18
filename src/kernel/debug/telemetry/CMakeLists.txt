# SPDX-License-Identifier: BSD-3-Clause

add_local_sources_ifdef(CONFIG_SOF_TELEMETRY sof telemetry.c)
add_local_sources_ifdef(CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS sof performance_monitor.c)
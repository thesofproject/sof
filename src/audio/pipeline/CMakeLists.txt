# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
  pipeline-graph.c
  pipeline-stream.c
  pipeline-params.c
  pipeline-xrun.c
  pipeline-schedule.c
)

add_local_sources_ifdef(CONFIG_PLATFORM_ESP32P4 sof
  sof_static_pipeline.c
  static_pipeline_loader.c
)

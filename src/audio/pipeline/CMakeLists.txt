# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
  pipeline-graph.c
  pipeline-stream.c
  pipeline-params.c
  pipeline-xrun.c
  pipeline-schedule.c
)

add_local_sources_ifdef(CONFIG_STATIC_PIPELINE sof
  static_pipeline_loader.c
  static_pipeline_modules.c
  static_pipeline_uac2.c
)

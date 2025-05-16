# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_TRACE)
  add_local_sources(sof dma-trace.c trace.c)
endif()

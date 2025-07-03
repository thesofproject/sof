# SPDX-License-Identifier: BSD-3-Clause

set(common_files notifier.c dma.c dai.c)

if(CONFIG_LIBRARY)
  add_local_sources(sof
    lib.c
    agent.c
    ${common_files}
    )
  return()
endif()

if(CONFIG_HAVE_AGENT)
  add_local_sources(sof agent.c)
endif()

if(CONFIG_AMS)
  add_local_sources(sof ams.c)
endif()

if(CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL)
  add_local_sources(sof cpu-clk-manager.c)
endif()

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  add_local_sources(sof ${common_files})

  if(NOT CONFIG_SOF_ZEPHYR_NO_SOF_CLOCK)
    add_local_sources(sof clk.c)
  endif()

endif()

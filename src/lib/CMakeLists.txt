# SPDX-License-Identifier: BSD-3-Clause

set(common_files notifier.c dma.c dai.c objpool.c)

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

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  add_local_sources(sof ${common_files})

endif()

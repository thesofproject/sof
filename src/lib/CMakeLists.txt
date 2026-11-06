# SPDX-License-Identifier: BSD-3-Clause

set(common_files notifier.c dma.c dai.c objpool.c)

if(CONFIG_LIBRARY)
  add_local_sources(sof
    lib.c
    ${common_files}
    )
  return()
endif()

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  add_local_sources(sof ${common_files})

endif()

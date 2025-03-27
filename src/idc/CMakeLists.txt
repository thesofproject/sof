# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MULTICORE)

  add_local_sources(sof idc.c)

  is_zephyr(zephyr)
  if(zephyr)
    add_local_sources(sof zephyr_idc.c)
  endif()

endif()

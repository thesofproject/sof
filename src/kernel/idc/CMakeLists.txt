# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr)
  add_local_sources_ifdef(CONFIG_SMP sof idc.c)
  add_local_sources(sof zephyr_idc.c)
endif()

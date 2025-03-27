# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY_MANAGER)
  add_local_sources(sof lib_manager.c lib_notification.c)

  if (CONFIG_MM_DRV AND CONFIG_LLEXT)
    add_local_sources(sof llext_manager.c)
  endif()
endif()

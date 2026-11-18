# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY_MANAGER)
  add_local_sources(sof lib_manager.c lib_notification.c)

  if (CONFIG_MM_DRV AND CONFIG_LLEXT)
    add_local_sources(sof llext_manager.c)
    if(CONFIG_LLEXT_EXPERIMENTAL AND NOT CONFIG_ADSP_IMR_CONTEXT_SAVE)
      add_local_sources_ifdef(CONFIG_L3_HEAP sof llext_manager_dram.c)
    endif()
  endif()
endif()

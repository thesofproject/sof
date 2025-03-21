# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof smart_amp.c smart_amp_generic.c)

if(CONFIG_MAXIM_DSM)
  add_local_sources(sof smart_amp_maxim_dsm.c)
  if(CONFIG_MAXIM_DSM_STUB)
    add_local_sources(sof maxim_dsm_stub.c)
  endif()
else()
  add_local_sources(sof smart_amp_passthru.c)
endif()

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  if(CONFIG_MAXIM_DSM)
    if(NOT CONFIG_MAXIM_DSM_STUB)
      zephyr_library_import(dsm ${CMAKE_CURRENT_LIST_DIR}/lib/release/dsm_lib/libdsm.a)
    endif()

    zephyr_include_directories(CONFIG_MAXIM_DSM ${CMAKE_CURRENT_LIST_DIR}/include/dsm_api/inc/)
  endif()

else() ### XTOS

  if(CONFIG_MAXIM_DSM)
    if(NOT CONFIG_MAXIM_DSM_STUB)
      sof_add_static_library(dsm ${CMAKE_CURRENT_LIST_DIR}/lib/release/dsm_lib/libdsm.a)
    endif()

    target_include_directories(sof PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include/dsm_api/inc)
  endif()

endif()

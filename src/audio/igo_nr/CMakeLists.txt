# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_IGO_NR STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/igo_nr_llext)
  add_dependencies(app igo_nr)
  return()
endif()

add_local_sources(sof igo_nr.c)

if (CONFIG_COMP_IGO_NR_STUB)
  add_local_sources(sof igo_nr_stub.c)

else()
  is_zephyr(zephyr)
  if(zephyr) ###  Zephyr ###
    zephyr_library_import(IGOChrome ${CMAKE_CURRENT_LIST_DIR}/libigonr.a)
  else()
    sof_add_static_library(IGOChrome ${CMAKE_CURRENT_LIST_DIR}/libigonr.a)
  endif()

endif()

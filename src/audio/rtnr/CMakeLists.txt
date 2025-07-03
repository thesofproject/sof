# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_RTNR STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/rtnr_llext)
  add_dependencies(app rtnr)
  return()
endif()

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  zephyr_library_sources(rtnr.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_RTNR_STUB rtnr_stub.c)

else() ### library, e.g. testbench or plugin ###

  add_local_sources(sof rtnr.c)
  if (CONFIG_COMP_RTNR_STUB)
    add_local_sources(sof rtnr_stub.c)
  endif()

endif()

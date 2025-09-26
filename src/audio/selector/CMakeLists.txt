# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_SEL STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/selector_llext)
  add_dependencies(app selector)
  return()
endif()

add_local_sources(sof selector_generic.c selector.c)

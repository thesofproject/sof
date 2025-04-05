# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_TEMPLATE_COMP STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/template_comp_llext)
  add_dependencies(app template_comp)
else()
  add_local_sources(sof template_comp.c)
endif()

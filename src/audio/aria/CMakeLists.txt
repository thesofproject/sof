# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_ARIA STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/aria_llext)
  add_dependencies(app aria)
else()
  add_local_sources(sof aria.c aria_hifi5.c aria_hifi3.c aria_generic.c)
endif()

# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_STEREOWIDEN STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/stereowiden_llext)
  add_dependencies(app stereowiden)
else()
  add_local_sources(sof stereowiden.c)
  if(CONFIG_COMP_STEREOWIDEN_STUB)
    add_local_sources(sof stereowiden-stub.c)
  else()
    add_local_sources(sof stereowiden-widen.c)
  endif()
endif()

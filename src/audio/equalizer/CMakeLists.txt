# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_EQUALIZER STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/equalizer_llext)
  add_dependencies(app equalizer)
else()
  add_local_sources(sof equalizer.c)
  if(CONFIG_COMP_EQUALIZER_STUB)
    add_local_sources(sof equalizer-stub.c)
  else()
    add_local_sources(sof equalizer-filter.c)
  endif()
endif()

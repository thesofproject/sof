# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_ACOMPRESSOR STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/acompressor_llext)
  add_dependencies(app acompressor)
else()
  add_local_sources(sof acompressor.c)
  if(CONFIG_COMP_ACOMPRESSOR_STUB)
    add_local_sources(sof acompressor-stub.c)
  else()
    add_local_sources(sof acompressor-compress.c)
  endif()
endif()

# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_TONE STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/tone_llext)
  add_dependencies(app tone)
else()
  add_local_sources(sof tone.c tone-ipc4.c)
endif()

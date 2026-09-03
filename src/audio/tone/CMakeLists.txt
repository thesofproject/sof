# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_TONE STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/tone_llext)
  add_dependencies(app tone)
else()
  if(CONFIG_IPC_MAJOR_3)
    add_local_sources(sof tone.c tone-ipc3.c)
  elseif(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof tone.c tone-ipc4.c)
  endif()
endif()

# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_TDFB STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/tdfb_llext)
  add_dependencies(app tdfb)
else()
  add_local_sources(sof tdfb.c tdfb_generic.c tdfb_hifiep.c tdfb_hifi3.c tdfb_direction.c)
  if(CONFIG_IPC_MAJOR_3)
    add_local_sources(sof tdfb_ipc3.c)
  elseif(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof tdfb_ipc4.c)
  endif()
endif()

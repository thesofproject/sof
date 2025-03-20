# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_MULTIBAND_DRC STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/multiband_drc_llext)
  add_dependencies(app multiband_drc)
else()
  add_local_sources(sof multiband_drc.c)
  add_local_sources(sof multiband_drc_generic.c)

  if(CONFIG_IPC_MAJOR_3)
    add_local_sources(sof multiband_drc_ipc3.c)
  elseif(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof multiband_drc_ipc4.c)
  endif()
endif()

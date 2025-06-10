# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_TEMPLATE_COMP STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/sound_dose_llext)
  add_dependencies(app sound_dose)
else()
  add_local_sources(sof sound_dose.c)
  add_local_sources(sof sound_dose-generic.c)

  if(CONFIG_IPC_MAJOR_3)
    add_local_sources(sof sound_dose-ipc3.c)
  elseif(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof sound_dose-ipc4.c)
  endif()
endif()

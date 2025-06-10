# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_SOUND_DOSE STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/sound_dose_llext)
  add_dependencies(app sound_dose)
else()
  add_local_sources(sof sound_dose.c)
  add_local_sources(sof sound_dose-generic.c)
  add_local_sources(sof sound_dose-ipc4.c)
endif()

# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_LEVEL_MULTIPLIER STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/level_multiplier_llext)
  add_dependencies(app level_multiplier)
else()
  add_local_sources(sof level_multiplier.c)
  add_local_sources(sof level_multiplier-generic.c)

  if(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof level_multiplier-ipc4.c)
  endif()
endif()

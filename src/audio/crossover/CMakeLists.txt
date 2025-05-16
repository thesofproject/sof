# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_CROSSOVER STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/crossover_llext)
  add_dependencies(app crossover)
  return()
endif()

add_local_sources(sof crossover.c)
add_local_sources(sof crossover_generic.c)
if(CONFIG_IPC_MAJOR_3)
	add_local_sources(sof crossover_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
	add_local_sources(sof crossover_ipc4.c)
endif()

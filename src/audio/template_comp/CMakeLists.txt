# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_TEMPLATE_COMP STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/template_comp_llext)
  add_dependencies(app template_comp)
else()
  add_local_sources(sof template.c)
  add_local_sources(sof template-generic.c)

  if(CONFIG_IPC_MAJOR_3)
    add_local_sources(sof template-ipc3.c)
  elseif(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof template-ipc4.c)
  endif()
endif()

# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_DRC STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/drc_llext)
  add_dependencies(app drc)
else()
  add_local_sources(sof
    drc.c
    drc_generic.c
    drc_hifi3.c
    drc_hifi4.c
    drc_math_generic.c
    drc_math_hifi3.c
    )
endif()

if(NOT CONFIG_COMP_DRC STREQUAL "n")
  add_local_sources(sof drc_log.c)
endif()

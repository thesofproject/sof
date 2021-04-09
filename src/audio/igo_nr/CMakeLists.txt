# SPDX-License-Identifier: BSD-3-Clause

target_include_directories(sof_public_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_local_sources(sof igo_nr.c)
sof_add_static_library(IGOChrome ${CMAKE_CURRENT_LIST_DIR}/libigonr.a)

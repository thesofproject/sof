# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof igo_nr.c)
sof_add_static_library(IGOChrome ${CMAKE_CURRENT_LIST_DIR}/libigonr.a)

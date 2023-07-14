# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof igo_nr.c)
if (CONFIG_COMP_IGO_NR_STUB)
add_local_sources(sof igo_nr_stub.c)
else ()
sof_add_static_library(IGOChrome ${CMAKE_CURRENT_LIST_DIR}/libigonr.a)
endif ()

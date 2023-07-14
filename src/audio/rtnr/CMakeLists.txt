# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof rtnr.c)

if (CONFIG_COMP_RTNR_STUB)
	add_local_sources(sof, rtnr_stub.c)
elseif(CONFIG_TIGERLAKE)
	add_subdirectory(rtklib/tgl)
elseif(CONFIG_MT8195)
	add_subdirectory(rtklib/mt8195)
endif()

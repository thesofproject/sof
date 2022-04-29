# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof rtnr.c)

if(CONFIG_TIGERLAKE)
	add_subdirectory(rtklib/tgl)
elseif(CONFIG_MT8195)
	add_subdirectory(rtklib/mt8195)
endif()

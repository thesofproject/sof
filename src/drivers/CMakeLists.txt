# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_IMX)
	add_subdirectory(imx)
endif()

if(CONFIG_INTEL)
	add_subdirectory(intel)
endif()

if(CONFIG_RENOIR OR CONFIG_REMBRANDT)
	add_subdirectory(amd)
endif()

if(CONFIG_MEDIATEK)
	add_subdirectory(mediatek)
endif()

if(CONFIG_LIBRARY)
	add_subdirectory(host)
	return()
endif()

if(CONFIG_DW)
	add_subdirectory(dw)
endif()

add_subdirectory(generic)

add_local_sources(sof interrupt.c)

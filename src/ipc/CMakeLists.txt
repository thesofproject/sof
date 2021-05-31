# SPDX-License-Identifier: BSD-3-Clause

if (CONFIG_IPC_MAJOR_3)
	add_subdirectory(ipc3)
elseif (CONFIG_IPC_MAJOR_4)
	add_subdirectory(ipc4)
endif()

add_local_sources(sof
	ipc-common.c
)

if (CONFIG_TRACE)
	add_local_sources(sof
		dma-copy.c)
endif()

if (CONFIG_LIBRARY)
	return()
endif()

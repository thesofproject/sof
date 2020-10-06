# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY)
	add_local_sources(sof
		ipc.c
	)
	return()
endif()

add_local_sources(sof
	ipc.c
	handler.c
)

if (CONFIG_TRACE)
	add_local_sources(sof
		dma-copy.c)
endif()

if (CONFIG_HOST_PTABLE)
	add_local_sources(sof
		ipc-host-ptable.c)
endif()

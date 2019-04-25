if(BUILD_HOST)
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

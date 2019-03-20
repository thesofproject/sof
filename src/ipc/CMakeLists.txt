if(BUILD_HOST)
	add_library(sof_ipc STATIC "")
	target_link_libraries(sof_ipc PRIVATE sof_options)
	add_local_sources(sof_ipc
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

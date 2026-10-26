add_local_sources(sof copier.c copier_hifi.c copier_generic.c copier_host.c)
if(NOT CONFIG_LIBRARY)
	add_local_sources(sof copier_dai.c)
endif()
if(CONFIG_IPC4_GATEWAY)
	add_local_sources(sof
		copier_ipcgtw.c
	)
endif()

if(CONFIG_COPIER_GAIN)
	add_local_sources(sof copier_gain.c)
endif()

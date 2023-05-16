add_local_sources(sof copier.c copier_hifi.c copier_generic.c)
if(CONFIG_IPC4_GATEWAY)
	add_local_sources(sof
		copier_ipcgtw.c
	)
endif()

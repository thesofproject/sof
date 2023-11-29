add_local_sources(sof multiband_drc.c)
add_local_sources(sof multiband_drc_generic.c)

if(CONFIG_IPC_MAJOR_3)
	add_local_sources(sof multiband_drc_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
	add_local_sources(sof multiband_drc_ipc4.c)
endif()

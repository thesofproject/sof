add_local_sources(sof crossover.c)
add_local_sources(sof crossover_generic.c)
if(CONFIG_IPC_MAJOR_3)
	add_local_sources(sof crossover_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
	add_local_sources(sof crossover_ipc4.c)
endif()

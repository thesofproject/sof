add_local_sources(sof
	hda-dma.c
	hda.c
	interrupt.c
	ssp.c
	timer.c
)

if(CONFIG_SUECREEK)
	add_local_sources(sof sue-ipc.c)
else()
	add_local_sources(sof ipc.c)
endif()

if(CONFIG_DMIC)
	add_local_sources(sof dmic.c)
endif()

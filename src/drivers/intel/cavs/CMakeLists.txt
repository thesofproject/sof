add_local_sources(sof
	hda-dma.c
	hda.c
	interrupt.c
	timer.c
)

if(CONFIG_SUECREEK)
	add_local_sources(sof sue-iomux.c sue-ipc.c)
else()
	add_local_sources(sof ipc.c)
endif()

if(CONFIG_CAVS_DMIC)
	add_local_sources(sof dmic.c)
endif()

if(CONFIG_CAVS_SSP)
	add_local_sources(sof ssp.c)
endif()

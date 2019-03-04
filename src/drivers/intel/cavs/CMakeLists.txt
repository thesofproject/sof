add_local_sources(sof
	hda-dma.c
	hda.c
	interrupt.c
	ssp.c
	timer.c
)

if(CONFIG_INTEL_IOMUX)
	add_local_sources(sof sue-iomux.c)
endif()

if(CONFIG_SUECREEK)
	add_local_sources(sof sue-ipc.c)
else()
	add_local_sources(sof ipc.c)
endif()

if(CONFIG_CAVS_DMIC)
	add_local_sources(sof dmic.c)
endif()

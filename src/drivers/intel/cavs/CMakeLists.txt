add_local_sources(sof
	dmic.c
	hda-dma.c
	hda.c
	interrupt.c
	ssp.c
	timer.c
)

if(CONFIG_SUECREEK)
	add_local_sources(sof sue-iomux.c sue-ipc.c)
else()
	add_local_sources(sof ipc.c)
endif()

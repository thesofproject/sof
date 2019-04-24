if(CONFIG_DW_DMA)
	add_local_sources(sof dma.c)
endif()

if(CONFIG_SUECREEK)
	add_local_sources(sof gpio.c ssi-spi.c)
endif()

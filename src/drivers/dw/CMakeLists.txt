add_local_sources(sof dma.c)

if(CONFIG_SUECREEK)
	add_local_sources(sof gpio.c ssi-spi.c)
endif()

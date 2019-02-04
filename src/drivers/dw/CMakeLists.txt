add_local_sources(sof dma.c)

if(CONFIG_SUECREEK)
	add_local_sources(sof gpio.c ssi-spi.c)
endif()

if(CONFIG_DW_UART)
	add_local_sources(sof uart.c uart-write-word.c)
	add_local_sources(bootloader uart-ll.c uart-write-word.c)
endif()

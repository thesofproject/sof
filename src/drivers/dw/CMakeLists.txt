# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_DW_DMA)
	add_local_sources(sof dma.c)
endif()

if(CONFIG_DW_GPIO)
	add_local_sources(sof gpio.c)
endif()

if(CONFIG_DW_SPI)
	add_local_sources(sof ssi-spi.c)
endif()

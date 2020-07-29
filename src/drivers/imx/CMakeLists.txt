# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	esai.c
	sai.c
	ipc.c
	timer.c
)

if(CONFIG_IMX_IRQSTEER)
	add_local_sources(sof interrupt.c)
endif()

if(CONFIG_IMX_EDMA)
	add_local_sources(sof edma.c)
endif()

if(CONFIG_IMX_SDMA)
	add_local_sources(sof sdma.c)
endif()

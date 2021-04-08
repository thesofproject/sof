# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	esai.c
	sai.c
	ipc.c
	timer.c
	sdma.c
)

if(CONFIG_IMX_EDMA)
	add_local_sources(sof edma.c)
endif()

if(CONFIG_IMX_INTERRUPT_IRQSTEER)
	add_local_sources(sof interrupt-irqsteer.c)
endif()

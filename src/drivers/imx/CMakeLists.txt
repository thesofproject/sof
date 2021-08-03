# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	sai.c
	ipc.c
	timer.c
	sdma.c
)

if(CONFIG_IMX_EDMA)
	add_local_sources(sof edma.c)
endif()

if(CONFIG_IMX_ESAI)
	add_local_sources(sof esai.c)
endif()

if(CONFIG_IMX_INTERRUPT_IRQSTEER)
	add_local_sources(sof interrupt-irqsteer.c)
elseif(CONFIG_IMX_INTERRUPT_GENERIC)
	add_local_sources(sof interrupt-generic.c)
endif()

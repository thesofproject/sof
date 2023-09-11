# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	sai.c
	ipc.c
	timer.c
)

if(CONFIG_IMX_EDMA)
	add_local_sources(sof edma.c)
endif()

if(CONFIG_IMX_ESAI)
	add_local_sources(sof esai.c)
endif()

if(CONFIG_IMX_SDMA)
	add_local_sources(sof sdma.c)

	if (CONFIG_HAVE_SDMA_FIRMWARE)
		target_include_directories(sof PRIVATE ${CONFIG_SDMA_SCRIPT_CODE})
	endif()
endif()

if(CONFIG_IMX_MICFIL)
	add_local_sources(sof micfil.c)
endif()

if(CONFIG_IMX_INTERRUPT_IRQSTEER)
	add_local_sources(sof interrupt-irqsteer.c)
elseif(CONFIG_IMX_INTERRUPT_GENERIC)
	add_local_sources(sof interrupt-generic.c)
endif()

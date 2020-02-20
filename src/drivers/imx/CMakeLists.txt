# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	esai.c
	sai.c
	interrupt.c
	ipc.c
	timer.c
)

if(CONFIG_IMX_EDMA)
	add_local_sources(sof edma.c)
endif()

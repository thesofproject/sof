# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_SCHEDULE_DMA_MULTI_CHANNEL)
	add_local_sources(sof dma_multi_chan_domain.c)
endif()

if(CONFIG_SCHEDULE_DMA_SINGLE_CHANNEL)
	add_local_sources(sof dma_single_chan_domain.c)
endif()

add_local_sources(sof
	edf_schedule.c
	ll_schedule.c
	schedule.c
	task.c
	timer_domain.c
)

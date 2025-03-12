# SPDX-License-Identifier: BSD-3-Clause

set(base_files
	schedule.c
)

if(CONFIG_SCHEDULE_DMA_MULTI_CHANNEL AND NOT(CONFIG_DMA_DOMAIN))
	list(APPEND base_files dma_multi_chan_domain.c)
endif()

if(CONFIG_SCHEDULE_DMA_SINGLE_CHANNEL AND NOT(CONFIG_DMA_DOMAIN))
	list(APPEND base_files dma_single_chan_domain.c)
endif()

is_zephyr(it_is)
if(it_is) ###  Zephyr ###

zephyr_library_sources(
	${base_files}
	zephyr_domain.c
)

if (CONFIG_SOC_ACP_6_0)
	# Zephyr DMA domain should only be used with zephyr_ll
	if (CONFIG_DMA_DOMAIN)
		zephyr_library_sources(
			zephyr_ll.c
			zephyr_dma_domain.c
		)
	else()
		zephyr_library_sources(
			ll_schedule.c
		)
	endif()
else()
	zephyr_library_sources(
		zephyr_ll.c
	)
endif()

zephyr_library_sources_ifdef(CONFIG_ZEPHYR_DP_SCHEDULER
	${SOF_SRC_PATH}/schedule/zephyr_dp_schedule.c
)

zephyr_library_sources_ifdef(CONFIG_ZEPHYR_TWB_SCHEDULER
	${SOF_SRC_PATH}/schedule/zephyr_twb_schedule.c
)

else() ###  Not Zephyr ###

add_local_sources(sof
	${base_files}
	edf_schedule.c
	ll_schedule.c
	task.c
	timer_domain.c
)

endif()

# SPDX-License-Identifier: BSD-3-Clause

if (CONFIG_IPC_MAJOR_3)
	add_subdirectory(ipc3)
elseif (CONFIG_IPC_MAJOR_4)
        add_subdirectory(ipc4)
endif()

# Common to Zephyr and XTOS
add_local_sources(sof
	ipc-common.c
	ipc-helper.c
	notification_pool.c
)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

# dma-copy only used for dma-trace
zephyr_library_sources_ifdef(CONFIG_TRACE dma-copy.c)

if (CONFIG_SOC_SERIES_INTEL_CAVS_V25 OR CONFIG_SOC_SERIES_INTEL_ADSP_ACE)
	zephyr_library_sources(
		ipc-zephyr.c
	)
endif()

else() ###  Not Zephyr ###

if (CONFIG_TRACE)
	add_local_sources(sof
		dma-copy.c)
endif()

endif() # Zephyr

# SPDX-License-Identifier: BSD-3-Clause

if (CONFIG_IPC_MAJOR_3)
	add_subdirectory(ipc3)
elseif (CONFIG_IPC_MAJOR_4)
	add_subdirectory(ipc4)
endif()

# Files common to Zephyr and the host library
add_local_sources(sof notification_pool.c)

is_zephyr(zephyr)
if(zephyr)
	if (CONFIG_SOC_SERIES_INTEL_CAVS_V25 OR CONFIG_SOC_SERIES_INTEL_ADSP_ACE)
		zephyr_library_sources(ipc-zephyr.c)
	endif()
endif()

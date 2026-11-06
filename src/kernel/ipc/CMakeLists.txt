# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr)
	if (CONFIG_SOC_SERIES_INTEL_CAVS_V25 OR CONFIG_SOC_SERIES_INTEL_ADSP_ACE)
		zephyr_library_sources(ipc-zephyr.c)
	endif()
endif()

# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof ssp.c)

if(CONFIG_INTEL_MCLK)
	add_local_sources(sof mn.c)
endif()

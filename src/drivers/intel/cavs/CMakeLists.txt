# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	hda-dma.c
	hda.c
	interrupt.c
	timer.c
)

if(CONFIG_INTEL_IOMUX)
	add_local_sources(sof sue-iomux.c)
endif()

if(CONFIG_SUECREEK)
	add_local_sources(sof sue-ipc.c)
else()
	add_local_sources(sof ipc.c)
endif()

if(CONFIG_CAVS_DMIC)
	add_local_sources(sof dmic.c)
endif()

if(CONFIG_CAVS_SSP)
	add_local_sources(sof ssp.c)
endif()

if(CONFIG_CAVS_MN)
	add_local_sources(sof mn.c)
endif()

if(CONFIG_CAVS_ALH)
	add_local_sources(sof alh.c)
endif()

# Build IDC driver only when not building in Zephyr
if(CONFIG_MULTICORE AND NOT CONFIG_SOC_FAMILY_INTEL_ADSP)
	add_local_sources(sof idc.c)
endif()

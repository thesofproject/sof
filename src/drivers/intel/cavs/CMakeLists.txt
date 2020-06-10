# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	interrupt.c
	timer.c
	timestamp.c
)

if(CONFIG_INTEL_IOMUX)
	add_local_sources(sof sue-iomux.c)
endif()

if(CONFIG_SUECREEK)
	add_local_sources(sof sue-ipc.c)
else()
	add_local_sources(sof ipc.c)
endif()

if(CONFIG_MULTICORE)
	add_local_sources(sof idc.c)
endif()

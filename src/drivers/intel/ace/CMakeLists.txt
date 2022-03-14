# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	interrupt.c
	ipc.c
	timestamp.c
)

if(NOT CONFIG_ZEPHYR_SOF_MODULE)
	add_local_sources(sof timer.c)
endif()

if(CONFIG_MULTICORE)
	add_local_sources(sof idc.c)
endif()

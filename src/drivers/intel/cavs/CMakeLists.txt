# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	interrupt.c
	ipc.c
	timer.c
	timestamp.c
)

if(CONFIG_MULTICORE)
	add_local_sources(sof idc.c)
endif()

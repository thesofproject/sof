# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof notifier.c)

if (CONFIG_MULTICORE)
	add_local_sources(sof cpu.c)
endif()

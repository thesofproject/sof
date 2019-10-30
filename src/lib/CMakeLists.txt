# SPDX-License-Identifier: BSD-3-Clause

if(BUILD_LIBRARY)
	add_local_sources(sof
		lib.c
		notifier.c)
	return()
endif()

if(CONFIG_HAVE_AGENT)
	add_local_sources(sof agent.c)
endif()

add_local_sources(sof
	lib.c
	alloc.c
	notifier.c
	pm_runtime.c
	clk.c
	dma.c
	dai.c
	wait.c
)

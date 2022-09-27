# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY)
	add_local_sources(sof
		lib.c
		dai.c
		dma.c
		notifier.c
                agent.c)
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
	cpu-clk-manager.c
)

if(CONFIG_AMS)
add_local_sources(sof ams.c)
endif()

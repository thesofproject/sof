# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	clk.c
	dai.c
	pm_runtime.c
)

if (CONFIG_SOME_FAKE)
	add_local_sources(sof memory.c)
endif()

if (CONFIG_TRACE)
	add_local_sources(sof trace.c)
endif()

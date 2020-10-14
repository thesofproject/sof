# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	alloc.c
	clk.c
	dai.c
	pm_runtime.c
)

if (CONFIG_TRACE)
	add_local_sources(sof trace.c)
endif()

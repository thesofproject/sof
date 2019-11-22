# SPDX-License-Identifier: BSD-3-Clause

add_library(pdown STATIC "")
target_link_libraries(pdown sof_options)
target_compile_options(pdown PRIVATE -mtext-section-literals)

add_local_sources(pdown power_down.S)
target_link_libraries(sof_static_libraries INTERFACE pdown)

add_local_sources(sof
	clk.c
	dai.c
	dma.c
	memory.c
	pm_runtime.c
	pm_memory.c
)

if(CONFIG_MEM_WND)
	add_local_sources(sof mem_window.c)
endif()

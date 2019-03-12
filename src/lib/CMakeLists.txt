if(BUILD_HOST)
	add_local_sources(tb_common lib.c )
	return()
endif()

add_local_sources(sof
	lib.c
	alloc.c
	ll_schedule.c
	notifier.c
	trace.c
	edf_schedule.c
	schedule.c
	agent.c
	interrupt.c
	dma-trace.c
	pm_runtime.c
	clk.c
	dma.c
	dai.c
	panic.c
	wait.c
)

// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Generic DSP initialisation. This calls architecture and platform specific
 * initialisation functions.
 */

#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/init.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/platform.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <ipc/trace.h>

/* main firmware context */
static struct sof sof;

int master_core_init(struct sof *sof)
{
	int err, i;

	/* init architecture */
	trace_point(TRACE_BOOT_ARCH);
	err = arch_init(sof);
	if (err < 0)
		panic(SOF_IPC_PANIC_ARCH);

	/* initialise system services */
	trace_point(TRACE_BOOT_SYS_HEAP);
	platform_init_memmap();
	init_heap(sof);

	interrupt_init();

#if CONFIG_TRACE
	trace_point(TRACE_BOOT_SYS_TRACES);
	trace_init(sof);
#endif

	trace_point(TRACE_BOOT_SYS_NOTIFIER);
	init_system_notify(sof);

	trace_point(TRACE_BOOT_SYS_POWER);
	pm_runtime_init();

	/* Turn off memory for all unused cores */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++)
		if (i != PLATFORM_MASTER_CORE_ID)
			pm_runtime_put(CORE_MEMORY_POW, i);

	/* init the platform */
	err = platform_init(sof);
	if (err < 0)
		panic(SOF_IPC_PANIC_PLATFORM);

	trace_point(TRACE_BOOT_PLATFORM);

	/* should not return */
	err = task_main_start(sof);

	return err;
}

int main(int argc, char *argv[])
{
	int err;

	enable_log(&err);
	trace_point(TRACE_BOOT_START);

	/* setup context */
	sof.argc = argc;
	sof.argv = argv;

	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID)
		err = master_core_init(&sof);
	else
		err = slave_core_init(&sof);

	/* should never get here */
	panic(SOF_IPC_PANIC_TASK);
	return err;
}

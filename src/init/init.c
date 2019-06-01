// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Generic DSP initialisation. This calls architecture and platform specific
 * initialisation functions.
 */

#include <stddef.h>
#include <sof/init.h>
#include <sof/task.h>
#include <sof/debug.h>
#include <sof/panic.h>
#include <sof/alloc.h>
#include <sof/notifier.h>
#include <sof/schedule.h>
#include <sof/trace.h>
#include <sof/dma-trace.h>
#include <sof/pm_runtime.h>
#include <sof/cpu.h>
#include <platform/idc.h>
#include <platform/platform.h>
#include <platform/memory.h>

/* main firmware context */
static struct sof sof;

int master_core_init(struct sof *sof)
{
	int err;

	/* init architecture */
	trace_point(TRACE_BOOT_ARCH);
	err = arch_init(sof);
	if (err < 0)
		panic(SOF_IPC_PANIC_ARCH);

	/* initialise system services */
	trace_point(TRACE_BOOT_SYS_HEAP);
	platform_init_memmap();
	init_heap(sof);

#if CONFIG_TRACE
	trace_init(sof);
#endif

	trace_point(TRACE_BOOT_SYS_NOTE);
	init_system_notify(sof);

	trace_point(TRACE_BOOT_SYS_POWER);
	pm_runtime_init();

	/* init the platform */
	err = platform_init(sof);
	if (err < 0)
		panic(SOF_IPC_PANIC_PLATFORM);

	trace_point(TRACE_BOOT_PLATFORM);

	/* should not return */
	err = do_task_master_core(sof);

	return err;
}

int main(int argc, char *argv[])
{
	int err;

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

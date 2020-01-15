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

struct sof *sof_get(void)
{
	return &sof;
}

int master_core_init(int argc, char *argv[], struct sof *sof)
{
	int err;

	/* setup context */
	sof->argc = argc;
	sof->argv = argv;

	/* init architecture */
	trace_point(TRACE_BOOT_ARCH);
	err = arch_init();
	if (err < 0)
		panic(SOF_IPC_PANIC_ARCH);

	/* initialise system services */
	trace_point(TRACE_BOOT_SYS_HEAP);
	platform_init_memmap(sof);
	init_heap(sof);

	interrupt_init(sof);

#if CONFIG_TRACE
	trace_point(TRACE_BOOT_SYS_TRACES);
	trace_init(sof);
#endif

	trace_point(TRACE_BOOT_SYS_NOTIFIER);
	init_system_notify(sof);

	trace_point(TRACE_BOOT_SYS_POWER);
	pm_runtime_init(sof);

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

	trace_point(TRACE_BOOT_START);

	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID)
		err = master_core_init(argc, argv, &sof);
	else
		err = slave_core_init(&sof);

	/* should never get here */
	panic(SOF_IPC_PANIC_TASK);
	return err;
}

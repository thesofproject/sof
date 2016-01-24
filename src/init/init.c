/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Generic DSP initialisation. This calls architecture and platform specific
 * initialisation functions. 
 */

#include <stddef.h>
#include <reef/init.h>
#include <reef/task.h>
#include <reef/debug.h>
#include <reef/ipc.h>
#include <reef/alloc.h>
#include <reef/notifier.h>
#include <reef/work.h>
#include <platform/platform.h>

int main(int argc, char *argv[])
{
	int err;

	/* init architecture */
	err = arch_init();
	if (err < 0)
		panic(PANIC_ARCH);

	/* initialise system services */
	init_heap();
	init_system_notify();
	init_system_workq();

	/* init the platform */
	err = platform_init();
	if (err < 0)
		panic(PANIC_PLATFORM);

	/* initialise the IPC mechanisms */
	err = platform_ipc_init();
	if (err < 0)
		panic(PANIC_IPC);

	/* should not return */
	err = do_task();

	/* should never get here */
	panic(PANIC_TASK);
	return err;
}

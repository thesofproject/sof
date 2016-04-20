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
#include <reef/alloc.h>
#include <reef/notifier.h>
#include <reef/work.h>
#include <reef/trace.h>
#include <platform/platform.h>

int main(int argc, char *argv[])
{
	int err;

	trace_point(TRACE_BOOT_START);

	/* init architecture */
	err = arch_init();
	if (err < 0)
		panic(PANIC_ARCH);

	trace_point(TRACE_BOOT_ARCH);

	/* initialise system services */
	init_heap();
	init_system_notify();

	trace_point(TRACE_BOOT_SYS);

	/* init the platform */
	err = platform_init();
	if (err < 0)
		panic(PANIC_PLATFORM);

	trace_point(TRACE_BOOT_PLATFORM);

	/* should not return */
	err = do_task();

	/* should never get here */
	panic(PANIC_TASK);
	return err;
}

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
#include <platform/platform.h>

int main(int argc, char *argv[])
{
	struct ipc *ipc_context;
	int err;

	dbg();

	/* init architecture */
	err = arch_init(argc, argv);
	if (err < 0)
		goto err_out;

	/* initialise the heap */
	init_heap();
	dbg();

	/* init the platform */
	err = platform_init(argc, argv);
	if (err < 0)
		goto err_out;

	/* init system work queue */
	init_system_workq();
	dbg();

	/* initialise the IPC mechanisms */
	ipc_context = ipc_init(NULL);
	if (ipc_context == NULL)
		panic(0);

	/* let host know DSP boot is complete */
	platform_boot_complete(0);

	/* should not return */
	err = do_task(argc, argv);

err_out:
	dbg();
	return err;
}

/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Generic audio task.
 */

#include <task.h>

int do_task(int argc, char *argv[])
{
	int ret = 0;

#if 0
	while (1)
	{
		ipc_process_queue();

		cpu_wait_for_irq();
	}
#endif
	/* something bad happened */
	return ret;
}

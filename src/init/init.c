/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Generic DSP initialisation. This calls architecture and platform specific
 * initialisation functions. 
 */

#include <reef/init.h>
#include <reef/task.h>

int main(int argc, char *argv[])
{
	int err;

	err = arch_init(argc, argv);
	if (err < 0)
		goto err_out;


	/* should not return */
	err = do_task(argc, argv);

err_out:
	return err;
}

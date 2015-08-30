/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Xtensa architecture initialisation.
 */

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <platform/memory.h>
#include <reef/mailbox.h>
#include <reef/debug.h>
#include <reef/init.h>
#include <stdint.h>

/* uniprocessor implementation */

void spinlock_lock(int lock)
{
}

void spinlock_unlock(int lock)
{
}

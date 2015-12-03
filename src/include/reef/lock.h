/*
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#ifndef __INCLUDE_LOCK__
#define __INCLUDE_LOCK__

#include <stdint.h>
#include <reef/interrupt.h>

typedef spinlock_t uint32_t;

/* uni-processor locking implementation using same syntax as Linux */
/* TODO: add multi-processor support */

#define spinlock_init(lock)

#define spinlock_lock(lock)
#define spinlock_unlock(lock)

#define spinlock_lock_irq(lock, flags) \
	interrupt_local_disable(flags); \
	spinlock_lock(lock);

#define spinlock_unlock_irq(lock, flags) \
	spinlock_unlock(lock); \
	interrupt_local_enable(flags);

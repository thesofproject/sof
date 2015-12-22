/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_WAIT__
#define __INCLUDE_WAIT__

void wait_for_interrupt(int level);

/* simple interrupt based wait for completion */
static inline void wait_for_completion(int *complete)
{
	volatile int *c = complete;

	/* check for completion after every wake from IRQ */
	while (*c == 0)
		wait_for_interrupt(0);
}

#endif

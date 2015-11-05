/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_CLOCK__
#define __INCLUDE_CLOCK__

void clock_enable(int clock);
void clock_disable(int clock);

void clock_set_freq(int clock, unsigned int hz);

unsigned int clock_get_freq(int clock);

#endif

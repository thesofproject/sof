/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_CLOCK__
#define __INCLUDE_CLOCK__

#include <reef/notifier.h>
#include <stdint.h>

#define CLOCK_NOTIFY_PRE	0
#define CLOCK_NOTIFY_POST	1

struct clock_notify_data {
	uint32_t old_freq;
	uint32_t new_freq;
};

void clock_enable(int clock);
void clock_disable(int clock);

unsigned int clock_set_freq(int clock, unsigned int hz);

unsigned int clock_get_freq(int clock);

unsigned int clock_ms_to_ticks(int clock, int ms);

void clock_register_notifier(int clock, struct notifier *notifier);

int clock_init();

#endif

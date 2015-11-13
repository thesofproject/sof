/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_TIMER__
#define __INCLUDE_TIMER__

int timer_register(int timer, void(*handler)(void *arg), void *arg);
void timer_unregister(int timer);

void timer_enable(int timer);
void timer_disable(int timer);

void timer_set(int timer, unsigned int ticks);

void timer_set_us(int timer, unsigned int ms);

unsigned int timer_get_count(int timer);

unsigned int timer_get_count_delta(int timer);

uint32_t timer_get_system(void);

#endif

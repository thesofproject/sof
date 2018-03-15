/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#ifndef __ARCH_TIMER_H_
#define __ARCH_TIMER_H_

#include <arch/interrupt.h>
#include <stdint.h>
#include <errno.h>

struct timer {
	uint32_t id;
	uint32_t irq;
	void *timer_data;	/* used by core */
	uint32_t hitime;	/* high end of 64bit timer */
	uint32_t hitimeout;
	uint32_t lowtimeout;
};

/* internal API calls */
int timer64_register(struct timer *timer, void(*handler)(void *arg), void *arg);
void timer_64_handler(void *arg);

static inline int arch_timer_register(struct timer *timer,
	void(*handler)(void *arg), void *arg)
{
	uint32_t flags;
	int ret;

	flags = arch_interrupt_global_disable();
	timer64_register(timer, handler, arg);
	ret = arch_interrupt_register(timer->id, timer_64_handler, timer);
	arch_interrupt_global_enable(flags);

	return ret;
}

static inline void arch_timer_unregister(struct timer *timer)
{
	arch_interrupt_unregister(timer->id);
}

static inline void arch_timer_enable(struct timer *timer)
{
	arch_interrupt_enable_mask(1 << timer->irq);
}

static inline void arch_timer_disable(struct timer *timer)
{
	arch_interrupt_disable_mask(1 << timer->irq);
}

uint64_t arch_timer_get_system(struct timer *timer);

int arch_timer_set(struct timer *timer, uint64_t ticks);

static inline void arch_timer_clear(struct timer *timer)
{
	arch_interrupt_clear(timer->irq);
}

#endif

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
 */

#ifndef __INCLUDE_INTERRUPT__
#define __INCLUDE_INTERRUPT__

#include <stdint.h>
#include <arch/interrupt.h>
#include <platform/interrupt.h>
#include <reef/trace.h>
#include <reef/debug.h>
#include <reef/lock.h>

#define trace_irq(__e)	trace_event(TRACE_CLASS_IRQ, __e)
#define trace_irq_error(__e)	trace_error(TRACE_CLASS_IRQ,  __e)

/* child interrupt source */
struct irq_child {
	uint32_t enabled;

	void (*handler)(void *arg);
	void *handler_arg;
};

/* parent source */
struct irq_parent {
	int num;
	void (*handler)(void *arg);
	uint32_t enabled_count;
	spinlock_t lock;

	uint32_t num_children;
	struct irq_child *child[PLATFORM_IRQ_CHILDREN];
};

int interrupt_register(uint32_t irq,
	void(*handler)(void *arg), void *arg);
void interrupt_unregister(uint32_t irq);
uint32_t interrupt_enable(uint32_t irq);
uint32_t interrupt_disable(uint32_t irq);

static inline void interrupt_set(int irq)
{
	arch_interrupt_set(REEF_IRQ_NUMBER(irq));
}

static inline void interrupt_clear(int irq)
{
	arch_interrupt_clear(REEF_IRQ_NUMBER(irq));
}

static inline uint32_t interrupt_global_disable(void)
{
	return arch_interrupt_global_disable();
}

static inline void interrupt_global_enable(uint32_t flags)
{
	arch_interrupt_global_enable(flags);
}

/* called by platform interrupt ops */
int irq_register_child(struct irq_parent *parent, int irq,
	void (*handler)(void *arg), void *arg);
void irq_unregister_child(struct irq_parent *parent, int irq);
uint32_t irq_enable_child(struct irq_parent *parent, int irq);
uint32_t irq_disable_child(struct irq_parent *parent, int irq);

#endif

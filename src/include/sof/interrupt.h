/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_INTERRUPT__
#define __INCLUDE_INTERRUPT__

#include <stdint.h>
#include <arch/interrupt.h>
#include <platform/interrupt.h>
#include <sof/drivers/interrupt.h>
#include <sof/trace.h>
#include <sof/debug.h>
#include <sof/lock.h>
#include <sof/list.h>

#define trace_irq(__e)	trace_event(TRACE_CLASS_IRQ, __e)
#define trace_irq_error(__e, ...) \
	trace_error(TRACE_CLASS_IRQ,  __e, ##__VA_ARGS__)

#define IRQ_MANUAL_UNMASK	0
#define IRQ_AUTO_UNMASK		1

struct irq_desc {
	/* irq must be first for constructor */
	int irq;        /* logical IRQ number */

	/* handler is optional for constructor */
	void (*handler)(void *arg);
	void *handler_arg;

	/* whether irq should be automatically unmasked */
	int unmask;

	/* to identify interrupt with the same IRQ */
	int id;
	spinlock_t lock;
	uint32_t enabled_count;

	/* to link to other irq_desc */
	struct list_item irq_list;

	uint32_t num_children;
	struct list_item child[PLATFORM_IRQ_CHILDREN];
};

int interrupt_register(uint32_t irq, int unmask, void(*handler)(void *arg),
		       void *arg);
void interrupt_unregister(uint32_t irq);
uint32_t interrupt_enable(uint32_t irq);
uint32_t interrupt_disable(uint32_t irq);

static inline void interrupt_set(int irq)
{
	arch_interrupt_set(SOF_IRQ_NUMBER(irq));
}

static inline void interrupt_clear(int irq)
{
	arch_interrupt_clear(SOF_IRQ_NUMBER(irq));
}

static inline uint32_t interrupt_global_disable(void)
{
	return arch_interrupt_global_disable();
}

static inline void interrupt_global_enable(uint32_t flags)
{
	arch_interrupt_global_enable(flags);
}

#endif

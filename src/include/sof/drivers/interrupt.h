/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_INTERRUPT_H__
#define __SOF_DRIVERS_INTERRUPT_H__

#include <arch/interrupt.h>
#include <platform/interrupt.h>
#include <sof/interrupt-map.h>
#include <sof/list.h>
#include <sof/spinlock.h>
#include <sof/trace.h>
#include <stdint.h>

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

void platform_interrupt_init(void);

struct irq_desc *platform_irq_get_parent(uint32_t irq);
void platform_interrupt_set(int irq);
void platform_interrupt_clear(uint32_t irq, uint32_t mask);
uint32_t platform_interrupt_get_enabled(void);
void platform_interrupt_mask(uint32_t irq, uint32_t mask);
void platform_interrupt_unmask(uint32_t irq, uint32_t mask);

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

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

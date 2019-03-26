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

/**
 * struct irq_child - child IRQ descriptor for cascading IRQ controllers
 *
 * @enable_count: IRQ enable counter
 * @list:	head for IRQ descriptors, sharing this interrupt
 */
struct irq_child {
	int enable_count;
	struct list_item list;
};

/**
 * struct irq_desc - interrupt client descriptor
 *
 * @irq:	virtual IRQ number
 * @handler:	interrupt handler function
 * @handler_arg: interrupt handler argument
 * @unmask:	whether irq should be automatically unmasked
 * @irq_list:	to link to other irq_desc
 */
struct irq_desc {
	int irq;
	void (*handler)(void *arg);
	void *handler_arg;
	int unmask;

	struct list_item irq_list;
};

/**
 * struct irq_cascade_ops - cascading IRQ controller operations
 *
 * @mask:	mask an interrupt
 * @unmask:	unmask an interrupt
 */
struct irq_cascade_ops {
	void (*mask)(struct irq_desc *desc, uint32_t irq);
	void (*unmask)(struct irq_desc *desc, uint32_t irq);
};

/**
 * struct irq_cascade_desc - cascading interrupt controller descriptor
 *
 * @name:	name of the controller
 * @irq_base:	first virtual IRQ number, assigned to this controller
 * @ops:	cascading interrupt controller driver operations
 * @desc:	the interrupt, that this controller is generating
 * @list:	link to the global list of interrupt controllers
 * @lock:	protect child lists in the below array
 * @enable_count: enabled child interrupt counter
 * @num_children: number of children
 * @child:	array of child lists - one per multiplexed IRQ
 */
struct irq_cascade_desc {
	/*
	 * The non-volatile part: doesn't change after initialisation, including
	 * the top part of .desc
	 */
	int irq_base;
	const char *name;
	const struct irq_cascade_ops *ops;

	struct irq_desc desc;

	/*
	 * The volatile part: changes after initialisation, including the bottom
	 * part of .desc above
	 */

	struct list_item list;

	spinlock_t lock;
	int enable_count;
	uint32_t num_children;
	struct irq_child child[PLATFORM_IRQ_CHILDREN];
};

/* A descriptor for cascading interrupt controller template */
struct irq_cascade_tmpl {
	const char *name;
	const struct irq_cascade_ops *ops;
	int irq;
	void (*handler)(void *arg);
};

int interrupt_register(uint32_t irq, int unmask, void(*handler)(void *arg),
		       void *arg);
void interrupt_unregister(uint32_t irq, const void *arg);
uint32_t interrupt_enable(uint32_t irq);
uint32_t interrupt_disable(uint32_t irq);

void interrupt_init(void);
int interrupt_cascade_register(const struct irq_cascade_tmpl *tmpl);
struct irq_cascade_desc *interrupt_get_parent(uint32_t irq);
int interrupt_get_irq(unsigned int irq, const char *cascade);

/*
 * On platforms, supporting cascading interrupts, interrupt numbers
 * 0...(PLATFORM_IRQ_CHILDREN - 1) are native DSP interrupts, larger interrupt
 * numbers are used for cascaded interrupts.
 */
#define interrupt_is_dsp_direct(irq) (!PLATFORM_IRQ_CHILDREN || \
					irq < PLATFORM_IRQ_CHILDREN)

static inline void interrupt_set(int irq)
{
	platform_interrupt_set(irq);
}

static inline void interrupt_clear_mask(int irq, uint32_t mask)
{
	platform_interrupt_clear(irq, mask);
}

static inline void interrupt_clear(int irq)
{
	interrupt_clear_mask(irq, 1);
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

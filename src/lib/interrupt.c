/*
 * Copyright (c) 2017, Intel Corporation
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
 * Author: Keyon Jie <yang.jie@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#include <sof/interrupt.h>
#include <sof/alloc.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <arch/interrupt.h>
#include <platform/interrupt.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * We have to invalidata cache of the list array, make sure no other data is
 * lost while doing that
 */
static struct {
	struct {
		int last_irq;
		struct list_item list[PLATFORM_CORE_COUNT];
	} s __attribute__((aligned(XCHAL_DCACHE_LINESIZE)));
	spinlock_t lock __attribute__((aligned(XCHAL_DCACHE_LINESIZE)));
} cascade_data;

/*
 * To avoid handling cache synchronisation manually when dealing with interrupts
 * on multiple DSP cores, we enforce a local IRQ policy. Only cascading
 * interrupt controller registration can be performed on an arbitrary core and
 * will register the controller for all the cores. All other interrupt related
 * functions must be called on the same core, where the interrupt is triggering.
 * This isn't a problem for us, because all our tasks are anyway bound to their
 * specific cores, no task migration is supported.
 */

static int interrupt_register_internal(uint32_t irq, int unmask,
				       void (*handler)(void *arg),
				       void *arg, struct irq_desc *desc);
static void interrupt_unregister_internal(uint32_t irq, const void *arg,
					  struct irq_desc *desc);

/* register cascading IRQ controllers for all cores */
int interrupt_cascade_register(const struct irq_cascade_tmpl *tmpl)
{
	struct irq_cascade_desc *cascade, *c;
	int core = cpu_get_id();
	struct list_item *list, *prev;
	unsigned long flags;
	unsigned int i, j;
	int ret;

	if (!tmpl->name || !tmpl->ops)
		return -EINVAL;

	/* Make sure not to currupt adjacent data when invalidating cache */
	cascade = (void *)((unsigned long)rmalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
					PLATFORM_CORE_COUNT * sizeof(*cascade) +
					XCHAL_DCACHE_LINESIZE - 1) &
			   ~(XCHAL_DCACHE_LINESIZE - 1));

	spin_lock_irq(&cascade_data.lock, flags);

	dcache_invalidate_region(&cascade_data.s, sizeof(cascade_data.s));

	/*
	 * All cascading interrupt controllers are always registered for all the
	 * cores, therefore it is sufficient to do all the checks with the
	 * current core to avoid cache problems
	 */
	list_for_item (list, cascade_data.s.list + core) {
		struct irq_cascade_desc *c = container_of(list,
						struct irq_cascade_desc, list);

		if (!rstrcmp(c->name, tmpl->name)) {
			ret = -EEXIST;
			trace_error(TRACE_CLASS_IRQ,
				    "error: cascading IRQ controller name duplication!");
			goto unlock;
		}
	}

	for (i = 0, c = cascade; i < PLATFORM_CORE_COUNT; i++, c++) {
		spinlock_init(&c->lock);
		for (j = 0; j < PLATFORM_IRQ_CHILDREN; j++)
			list_init(&c->child[j].list);

		c->name = tmpl->name;
		c->ops = tmpl->ops;
		c->desc.irq = tmpl->irq;
		c->desc.handler = tmpl->handler;
		c->desc.handler_arg = &c->desc;
		c->irq_base = cascade_data.s.last_irq + 1;

		prev = list_is_empty(cascade_data.s.list + i) ? NULL :
			cascade_data.s.list[i].prev;

		if (prev)
			dcache_invalidate_region(prev,
						 sizeof(struct list_item));
		list_item_append(&c->list, cascade_data.s.list + i);
		if (prev)
			dcache_writeback_region(prev, sizeof(struct list_item));
	}

	cascade_data.s.last_irq += ARRAY_SIZE(cascade[0].child);

	dcache_writeback_region(cascade,
				PLATFORM_CORE_COUNT * sizeof(*cascade));
	dcache_writeback_region(&cascade_data.s, sizeof(cascade_data.s));

	ret = 0;

unlock:
	spin_unlock_irq(&cascade_data.lock, flags);

	return ret;
}

int interrupt_get_irq(unsigned int irq, const char *cascade)
{
	struct list_item *list;
	unsigned long flags;
	unsigned int cpu = cpu_get_id();
	int ret = -ENODEV;

	if (!cascade || cascade[0] == '\0')
		return irq;

	/* If a name is specified, irq must be <= PLATFORM_IRQ_CHILDREN */
	if (irq >= PLATFORM_IRQ_CHILDREN) {
		trace_error(TRACE_CLASS_IRQ,
			    "error: IRQ %d invalid as a child interrupt!");
		return -EINVAL;
	}

	spin_lock_irq(&cascade_data.lock, flags);

	list_for_item (list, cascade_data.s.list + cpu) {
		struct irq_cascade_desc *c = container_of(list,
						struct irq_cascade_desc, list);

		/* .name is non-volatile */
		if (!rstrcmp(cascade, c->name)) {
			ret = c->irq_base + irq;
			break;
		}
	}

	spin_unlock_irq(&cascade_data.lock, flags);

	return ret;
}

struct irq_desc *interrupt_get_parent(uint32_t irq)
{
	struct list_item *list;
	struct irq_desc *parent = NULL;
	unsigned long flags;
	unsigned int cpu = cpu_get_id();

	if (interrupt_is_dsp_direct(irq))
		return NULL;

	spin_lock_irq(&cascade_data.lock, flags);

	list_for_item (list, cascade_data.s.list + cpu) {
		struct irq_cascade_desc *c = container_of(list,
						struct irq_cascade_desc, list);

		if (irq >= c->irq_base &&
		    irq < c->irq_base + PLATFORM_IRQ_CHILDREN) {
			parent = &c->desc;
			break;
		}
	}

	spin_unlock_irq(&cascade_data.lock, flags);

	return parent;
}

void interrupt_init(void)
{
	unsigned int i;

	spinlock_init(&cascade_data.lock);

	for (i = 0; i < ARRAY_SIZE(cascade_data.s.list); i++)
		list_init(cascade_data.s.list + i);

	cascade_data.s.last_irq = PLATFORM_IRQ_CHILDREN - 1;

	dcache_writeback_region(&cascade_data, sizeof(cascade_data));
}

static int irq_register_child(struct irq_desc *parent, int irq, int unmask,
		void (*handler)(void *arg), void *arg, struct irq_desc *desc)
{
	int hw_irq, ret = 0;
	struct irq_desc *child;
	struct irq_cascade_desc *cascade;
	struct list_item *list, *head;

	cascade = container_of(parent, struct irq_cascade_desc, desc);

	spin_lock(&cascade->lock);

	hw_irq = irq - cascade->irq_base;

	if (hw_irq < 0 || cascade->irq_base + PLATFORM_IRQ_CHILDREN <= irq) {
		ret = -EINVAL;
		goto finish;
	}

	head = &cascade->child[hw_irq].list;

	list_for_item (list, head) {
		child = container_of(list, struct irq_desc, irq_list);

		if (child->handler_arg == arg) {
			trace_error(TRACE_CLASS_IRQ,
				    "error: IRQ 0x%x handler argument re-used!",
				    irq);
			ret = -EEXIST;
			goto finish;
		}

		if (child->unmask != unmask) {
			trace_error(TRACE_CLASS_IRQ,
				    "error: IRQ 0x%x flags differ!", irq);
			ret = -EINVAL;
			goto finish;
		}
	}

	if (!desc) {
		/*
		 * init child from run-time, may be registered and unregistered
		 * many times at run-time
		 */
		child = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM,
				sizeof(struct irq_desc));
		if (!child) {
			ret = -ENOMEM;
			goto finish;
		}

		child->handler = handler;
		child->handler_arg = arg;
		child->unmask = unmask;
		child->irq = irq;
	} else {
		child = desc;
	}

	list_item_append(&child->irq_list, head);

	/* do we need to register parent ? */
	if (!cascade->num_children)
		ret = interrupt_register_internal(parent->irq, IRQ_AUTO_UNMASK,
					parent->handler, parent, parent);

	/* increment number of children */
	if (!ret)
		cascade->num_children++;

finish:
	spin_unlock(&cascade->lock);

	return ret;
}

static void irq_unregister_child(struct irq_desc *parent, int irq,
				 const void *arg, struct irq_desc *desc)
{
	struct irq_desc *child;
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);
	int hw_irq = irq - cascade->irq_base;
	struct list_item *list, *head = &cascade->child[hw_irq].list;

	spin_lock(&cascade->lock);

	list_for_item (list, head) {
		child = container_of(list, struct irq_desc, irq_list);

		if (child->handler_arg == arg) {
			list_item_del(&child->irq_list);
			cascade->num_children--;
			if (!desc)
				rfree(child);

			/*
			 * unregister the root interrupt if this child is the
			 * last registered one.
			 */
			if (!cascade->num_children)
				interrupt_unregister_internal(parent->irq,
							      parent, parent);

			break;
		}
	}

	spin_unlock(&cascade->lock);
}

static uint32_t irq_enable_child(struct irq_desc *parent, int irq)
{
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);
	struct irq_child *child = cascade->child + irq - cascade->irq_base;

	spin_lock(&cascade->lock);

	if (!child->enable_count++) {
		/* enable the parent interrupt */
		if (!cascade->enable_count++)
			interrupt_enable(parent->irq);

		/* enable the child interrupt */
		interrupt_unmask(irq);
	}

	spin_unlock(&cascade->lock);

	return 0;
}

static uint32_t irq_disable_child(struct irq_desc *parent, int irq)
{
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);
	struct irq_child *child = cascade->child + irq - cascade->irq_base;

	spin_lock(&cascade->lock);

	if (!child->enable_count) {
		trace_error(TRACE_CLASS_IRQ,
			    "error: IRQ %x unbalanced interrupt_disable()",
			    irq);
	} else if (!--child->enable_count) {
		/* disable the child interrupt */
		interrupt_mask(irq);

		/* disable the parent interrupt */
		if (!--cascade->enable_count)
			interrupt_disable(parent->irq);
	}

	spin_unlock(&cascade->lock);

	return 0;
}

int interrupt_register(uint32_t irq, int unmask, void (*handler)(void *arg),
		       void *arg)
{
	return interrupt_register_internal(irq, unmask, handler, arg, NULL);
}

static int interrupt_register_internal(uint32_t irq, int unmask,
				       void (*handler)(void *arg),
				       void *arg, struct irq_desc *desc)
{
	struct irq_desc *parent;

	/* no parent means we are registering DSP internal IRQ */
	parent = interrupt_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_register(irq, handler, arg);
	else
		return irq_register_child(parent, irq, unmask, handler, arg, desc);
}

void interrupt_unregister(uint32_t irq, const void *arg)
{
	interrupt_unregister_internal(irq, arg, NULL);
}

static void interrupt_unregister_internal(uint32_t irq, const void *arg,
					  struct irq_desc *desc)
{
	struct irq_desc *parent;

	/* no parent means we are unregistering DSP internal IRQ */
	parent = interrupt_get_parent(irq);
	if (parent == NULL)
		arch_interrupt_unregister(irq);
	else
		irq_unregister_child(parent, irq, arg, desc);
}

uint32_t interrupt_enable(uint32_t irq)
{
	struct irq_desc *parent;

	/* no parent means we are enabling DSP internal IRQ */
	parent = interrupt_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_enable_mask(1 << irq);
	else
		return irq_enable_child(parent, irq);
}

uint32_t interrupt_disable(uint32_t irq)
{
	struct irq_desc *parent;

	/* no parent means we are disabling DSP internal IRQ */
	parent = interrupt_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_disable_mask(1 << irq);
	else
		return irq_disable_child(parent, irq);
}

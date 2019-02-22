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
#include <sof/interrupt-map.h>
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
	struct list_item list[PLATFORM_CORE_COUNT]
		__attribute__((aligned(XCHAL_DCACHE_LINESIZE)));
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

	dcache_invalidate_region(cascade_data.list, sizeof(cascade_data.list));

	/*
	 * All cascading interrupt controllers are always registered for all the
	 * cores, therefore it is sufficient to do all the checks with the
	 * current core to avoid cache problems
	 */
	list_for_item (list, cascade_data.list + core) {
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
			list_init(&c->child[j]);

		c->name = tmpl->name;
		c->ops = tmpl->ops;
		c->desc.irq = tmpl->irq;
		c->desc.handler = tmpl->handler;
		c->desc.handler_arg = &c->desc;

		prev = list_is_empty(cascade_data.list + i) ? NULL :
			cascade_data.list[i].prev;

		if (prev)
			dcache_invalidate_region(prev,
						 sizeof(struct list_item));
		list_item_append(&c->list, cascade_data.list + i);
		if (prev)
			dcache_writeback_region(prev, sizeof(struct list_item));
	}

	dcache_writeback_region(cascade,
				PLATFORM_CORE_COUNT * sizeof(*cascade));
	dcache_writeback_region(cascade_data.list, sizeof(cascade_data.list));

	ret = 0;

unlock:
	spin_unlock_irq(&cascade_data.lock, flags);

	return ret;
}

struct irq_desc *interrupt_get_parent(uint32_t irq)
{
	struct list_item *list;
	struct irq_desc *parent = NULL;
	unsigned long flags;
	unsigned int cpu = SOF_IRQ_CPU(irq);

	if (irq < PLATFORM_IRQ_CHILDREN)
		return NULL;

	/* We should only be called for the current core */
	if (cpu != cpu_get_id())
		panic(SOF_IPC_PANIC_PLATFORM);

	spin_lock_irq(&cascade_data.lock, flags);

	list_for_item (list, cascade_data.list + cpu) {
		struct irq_cascade_desc *c = container_of(list,
						struct irq_cascade_desc, list);

		if (SOF_IRQ_NUMBER(irq) == c->desc.irq) {
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

	for (i = 0; i < ARRAY_SIZE(cascade_data.list); i++)
		list_init(cascade_data.list + i);

	dcache_writeback_region(cascade_data.list, sizeof(cascade_data.list));
}

static int irq_register_child(struct irq_desc *parent, int irq, int unmask,
			      void (*handler)(void *arg), void *arg)
{
	int ret = 0;
	struct irq_desc *child;
	struct irq_cascade_desc *cascade;
	struct list_item *list;

	if (parent == NULL)
		return -EINVAL;

	cascade = container_of(parent, struct irq_cascade_desc, desc);

	spin_lock(&cascade->lock);

	list_for_item (list, &cascade->child[SOF_IRQ_BIT(irq)]) {
		child = container_of(list, struct irq_desc, irq_list);

		if (child->handler_arg == arg) {
			trace_error(TRACE_CLASS_IRQ,
				    "error: IRQ 0x%x handler argument re-used!",
				    irq);
			ret = -EINVAL;
			goto finish;
		}
	}

	/* init child from run-time, may be registered and unregistered
	 * many times at run-time
	 */
	child = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM,
			sizeof(struct irq_desc));
	if (!child) {
		ret = -ENOMEM;
		goto finish;
	}

	child->enabled_count = 0;
	child->handler = handler;
	child->handler_arg = arg;
	child->id = SOF_IRQ_ID(irq);
	child->unmask = unmask;

	list_item_append(&child->irq_list, &cascade->child[SOF_IRQ_BIT(irq)]);

	/* do we need to register parent ? */
	if (cascade->num_children == 0) {
		ret = arch_interrupt_register(parent->irq,
					      parent->handler, parent);
	}

	/* increment number of children */
	cascade->num_children++;

finish:
	spin_unlock(&cascade->lock);
	return ret;
}

static void irq_unregister_child(struct irq_desc *parent, int irq,
				 const void *arg)
{
	struct irq_desc *child;
	struct list_item *clist;
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);

	spin_lock(&cascade->lock);

	/* does child already exist ? */
	if (list_is_empty(&cascade->child[SOF_IRQ_BIT(irq)]))
		goto finish;

	list_for_item (clist, &cascade->child[SOF_IRQ_BIT(irq)]) {
		child = container_of(clist, struct irq_desc, irq_list);

		if (child->handler_arg == arg) {
			list_item_del(&child->irq_list);
			cascade->num_children--;
			rfree(child);
			break;
		}
	}

	/*
	 * unregister the root interrupt if the this l2 is
	 * the last registered one.
	 */
	if (cascade->num_children == 0)
		arch_interrupt_unregister(parent->irq);

finish:
	spin_unlock(&cascade->lock);
}

static uint32_t irq_enable_child(struct irq_desc *parent, int irq)
{
	struct irq_desc *child;
	struct list_item *clist;
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);

	spin_lock(&cascade->lock);

	/* enable the parent interrupt */
	if (parent->enabled_count == 0)
		arch_interrupt_enable_mask(1 << parent->irq);

	list_for_item(clist, &cascade->child[SOF_IRQ_BIT(irq)]) {
		child = container_of(clist, struct irq_desc, irq_list);

		if ((SOF_IRQ_ID(irq) == child->id) &&
		    !child->enabled_count) {
			child->enabled_count = 1;
			parent->enabled_count++;

			/* enable the child interrupt */
			interrupt_unmask(irq);
		}
	}

	spin_unlock(&cascade->lock);
	return 0;

}

static uint32_t irq_disable_child(struct irq_desc *parent, int irq)
{
	struct irq_desc *child;
	struct list_item *clist;
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);

	spin_lock(&cascade->lock);

	list_for_item(clist, &cascade->child[SOF_IRQ_BIT(irq)]) {
		child = container_of(clist, struct irq_desc, irq_list);

		if ((SOF_IRQ_ID(irq) == child->id) &&
		    child->enabled_count) {
			child->enabled_count = 0;
			parent->enabled_count--;

			/* disable the child interrupt */
			interrupt_mask(irq);
		}
	}

	if (parent->enabled_count == 0)
		arch_interrupt_disable_mask(1 << parent->irq);

	spin_unlock(&cascade->lock);
	return 0;
}

int interrupt_register(uint32_t irq, int unmask, void (*handler)(void *arg),
		       void *arg)
{
	struct irq_desc *parent;

	/* no parent means we are registering DSP internal IRQ */
	parent = interrupt_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_register(irq, handler, arg);
	else
		return irq_register_child(parent, irq, unmask, handler, arg);
}

void interrupt_unregister(uint32_t irq, const void *arg)
{
	struct irq_desc *parent;

	/* no parent means we are unregistering DSP internal IRQ */
	parent = interrupt_get_parent(irq);
	if (parent == NULL)
		arch_interrupt_unregister(irq);
	else
		irq_unregister_child(parent, irq, arg);
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

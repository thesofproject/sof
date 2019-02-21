// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Keyon Jie <yang.jie@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/interrupt-map.h>
#include <sof/lib/alloc.h>
#include <sof/list.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static spinlock_t cascade_lock;
static union {
	struct irq_cascade_desc *list __aligned(PLATFORM_DCACHE_ALIGN);
	uint8_t bytes[PLATFORM_DCACHE_ALIGN];
} cascade_root;

int interrupt_cascade_register(const struct irq_cascade_tmpl *tmpl)
{
	struct irq_cascade_desc **cascade;
	unsigned long flags;
	unsigned int i;
	int ret;

	if (!tmpl->name)
		return -EINVAL;

	spin_lock_irq(&cascade_lock, flags);

	dcache_invalidate_region(&cascade_root, sizeof(cascade_root));

	for (cascade = &cascade_root.list; *cascade;
	     cascade = &(*cascade)->next) {
		if (!rstrcmp((*cascade)->name, tmpl->name)) {
			ret = -EEXIST;
			trace_error(TRACE_CLASS_IRQ,
				    "error: cascading IRQ controller name duplication!");
			goto unlock;
		}
	}

	*cascade = rmalloc(RZONE_SYS | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
			   sizeof(**cascade));

	spinlock_init(&(*cascade)->lock);
	for (i = 0; i < PLATFORM_IRQ_CHILDREN; i++)
		list_init(&(*cascade)->child[i]);

	(*cascade)->name = tmpl->name;
	(*cascade)->desc.irq = tmpl->irq;
	(*cascade)->desc.handler = tmpl->handler;
	(*cascade)->desc.handler_arg = &(*cascade)->desc;

	if (cascade == &cascade_root.list)
		/* First descriptor */
		dcache_writeback_region(&cascade_root, sizeof(cascade_root));

	ret = 0;

unlock:
	spin_unlock_irq(&cascade_lock, flags);

	return ret;
}

struct irq_desc *interrupt_get_parent(uint32_t irq)
{
	struct irq_cascade_desc *cascade;
	struct irq_desc *parent = NULL;
	unsigned long flags;

	if (irq < PLATFORM_IRQ_CHILDREN)
		return NULL;

	spin_lock_irq(&cascade_lock, flags);

	dcache_invalidate_region(&cascade_root, sizeof(cascade_root));

	for (cascade = cascade_root.list; cascade; cascade = cascade->next)
		if (SOF_IRQ_NUMBER(irq) == cascade->desc.irq) {
			parent = &cascade->desc;
			break;
		}

	spin_unlock_irq(&cascade_lock, flags);

	return parent;
}

void interrupt_init(void)
{
	spinlock_init(&cascade_lock);
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

	list_for_item(list, &cascade->child[SOF_IRQ_BIT(irq)]) {
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
	struct list_item *tlist;
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);

	spin_lock(&cascade->lock);

	/* does child already exist ? */
	if (list_is_empty(&cascade->child[SOF_IRQ_BIT(irq)]))
		goto finish;

	list_for_item_safe(clist, tlist, &cascade->child[SOF_IRQ_BIT(irq)]) {
		child = container_of(clist, struct irq_desc, irq_list);

		if (SOF_IRQ_ID(irq) == child->id) {
			if (child->handler_arg != arg)
				trace_error(TRACE_CLASS_IRQ,
					    "error: IRQ 0x%x handler argument mismatch!",
					    irq);
			list_item_del(&child->irq_list);
			cascade->num_children--;
			rfree(child);
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
		arch_interrupt_enable_mask(1 << SOF_IRQ_NUMBER(irq));

	list_for_item(clist, &cascade->child[SOF_IRQ_BIT(irq)]) {
		child = container_of(clist, struct irq_desc, irq_list);

		if ((SOF_IRQ_ID(irq) == child->id) &&
		    !child->enabled_count) {
			child->enabled_count = 1;
			parent->enabled_count++;

			/* enable the child interrupt */
			platform_interrupt_unmask(irq);
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
			platform_interrupt_mask(irq);
		}
	}

	if (parent->enabled_count == 0)
		arch_interrupt_disable_mask(1 << SOF_IRQ_NUMBER(irq));

	spin_unlock(&cascade->lock);
	return 0;
}

int interrupt_register(uint32_t irq, int unmask, void (*handler)(void *arg),
		       void *arg)
{
	struct irq_desc *parent;

	/* no parent means we are registering DSP internal IRQ */
	parent = platform_irq_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_register(irq, handler, arg);
	else
		return irq_register_child(parent, irq, unmask, handler, arg);
}

void interrupt_unregister(uint32_t irq, const void *arg)
{
	struct irq_desc *parent;

	/* no parent means we are unregistering DSP internal IRQ */
	parent = platform_irq_get_parent(irq);
	if (parent == NULL)
		arch_interrupt_unregister(irq);
	else
		irq_unregister_child(parent, irq, arg);
}

uint32_t interrupt_enable(uint32_t irq)
{
	struct irq_desc *parent;

	/* no parent means we are enabling DSP internal IRQ */
	parent = platform_irq_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_enable_mask(1 << irq);
	else
		return irq_enable_child(parent, irq);
}

uint32_t interrupt_disable(uint32_t irq)
{
	struct irq_desc *parent;

	/* no parent means we are disabling DSP internal IRQ */
	parent = platform_irq_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_disable_mask(1 << irq);
	else
		return irq_disable_child(parent, irq);
}

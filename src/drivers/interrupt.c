// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Keyon Jie <yang.jie@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* 1862d39a-3a84-4d64-8c91-dce1dfc122db */

DECLARE_SOF_UUID("irq", irq_uuid, 0x1862d39a, 0x3a84, 0x4d64,
		 0x8c, 0x91, 0xdc, 0xe1, 0xdf, 0xc1, 0x22, 0xdb);

DECLARE_TR_CTX(irq_tr, SOF_UUID(irq_uuid), LOG_LEVEL_INFO);

static SHARED_DATA struct cascade_root cascade_root;

static int interrupt_register_internal(uint32_t irq, void (*handler)(void *arg),
				       void *arg, struct irq_desc *desc);
static void interrupt_unregister_internal(uint32_t irq, const void *arg,
					  struct irq_desc *desc);

int interrupt_cascade_register(const struct irq_cascade_tmpl *tmpl)
{
	struct cascade_root *root = cascade_root_get();
	struct irq_cascade_desc **cascade;
	unsigned long flags;
	unsigned int i;
	int ret;

	if (!tmpl->name || !tmpl->ops)
		return -EINVAL;

	spin_lock_irq(&root->lock, flags);

	for (cascade = &root->list; *cascade;
	     cascade = &(*cascade)->next) {
		if (!rstrcmp((*cascade)->name, tmpl->name)) {
			ret = -EEXIST;
			tr_err(&irq_tr, "cascading IRQ controller name duplication!");
			goto unlock;
		}

	}

	*cascade = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(**cascade));

	spinlock_init(&(*cascade)->lock);

	for (i = 0; i < PLATFORM_IRQ_CHILDREN; i++)
		list_init(&(*cascade)->child[i].list);

	(*cascade)->name = tmpl->name;
	(*cascade)->ops = tmpl->ops;
	(*cascade)->global_mask = tmpl->global_mask;
	(*cascade)->irq_base = root->last_irq + 1;
	(*cascade)->desc.irq = tmpl->irq;
	(*cascade)->desc.handler = tmpl->handler;
	(*cascade)->desc.handler_arg = &(*cascade)->desc;
	(*cascade)->desc.cpu_mask = 1 << cpu_get_id();

	root->last_irq += ARRAY_SIZE((*cascade)->child);

	ret = 0;


unlock:

	spin_unlock_irq(&root->lock, flags);

	return ret;
}

int interrupt_get_irq(unsigned int irq, const char *name)
{
	struct cascade_root *root = cascade_root_get();
	struct irq_cascade_desc *cascade;
	unsigned long flags;
	int ret = -ENODEV;

	if (!name || name[0] == '\0')
		return irq;

	/* If a name is specified, irq must be <= PLATFORM_IRQ_CHILDREN */
	if (irq >= PLATFORM_IRQ_CHILDREN) {
		tr_err(&irq_tr, "IRQ %d invalid as a child interrupt!",
		       irq);
		return -EINVAL;
	}

	spin_lock_irq(&root->lock, flags);

	for (cascade = root->list; cascade; cascade = cascade->next) {
		/* .name is non-volatile */
		if (!rstrcmp(name, cascade->name)) {
			ret = cascade->irq_base + irq;
			break;
		}

	}


	spin_unlock_irq(&root->lock, flags);

	return ret;
}

struct irq_cascade_desc *interrupt_get_parent(uint32_t irq)
{
	struct cascade_root *root = cascade_root_get();
	struct irq_cascade_desc *cascade, *c = NULL;
	unsigned long flags;

	if (irq < PLATFORM_IRQ_HW_NUM)
		return NULL;

	spin_lock_irq(&root->lock, flags);

	for (cascade = root->list; cascade; cascade = cascade->next) {
		if (irq >= cascade->irq_base &&
		    irq < cascade->irq_base + PLATFORM_IRQ_CHILDREN) {
			c = cascade;
			break;
		}

	}


	spin_unlock_irq(&root->lock, flags);

	return c;
}

void interrupt_init(struct sof *sof)
{
	sof->cascade_root = platform_shared_get(&cascade_root,
						sizeof(cascade_root));

	sof->cascade_root->last_irq = PLATFORM_IRQ_FIRST_CHILD - 1;
	spinlock_init(&sof->cascade_root->lock);
}

static int irq_register_child(struct irq_cascade_desc *cascade, int irq,
			      void (*handler)(void *arg), void *arg,
			      struct irq_desc *desc)
{
	unsigned int core = cpu_get_id();
	struct irq_desc *child, *parent = &cascade->desc;
	struct list_item *list, *head;
	int hw_irq, ret = 0;

	hw_irq = irq - cascade->irq_base;

	if (hw_irq < 0 || cascade->irq_base + PLATFORM_IRQ_CHILDREN <= irq) {
		ret = -EINVAL;
		goto out;
	}

	head = &cascade->child[hw_irq].list;

	list_for_item(list, head) {
		child = container_of(list, struct irq_desc, irq_list);

		if (child->handler_arg == arg) {

			tr_err(&irq_tr, "IRQ 0x%x handler argument re-used!",
			       irq);
			ret = -EEXIST;
			goto out;
		}

	}

	if (!desc) {
		/* init child from run-time, may be registered and unregistered
		 * many times at run-time
		 */
		child = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
				sizeof(struct irq_desc));
		if (!child) {
			ret = -ENOMEM;
			goto out;
		}

		child->handler = handler;
		child->handler_arg = arg;
		child->irq = irq;
	} else {
		child = desc;
		child->cpu_mask = 0;
	}

	list_item_append(&child->irq_list, head);

	/* do we need to register parent on this CPU? */
	if (!cascade->num_children[core])
		ret = interrupt_register_internal(parent->irq, parent->handler,
						  parent, parent);

	/* increment number of children */
	if (!ret)
		cascade->num_children[core]++;


out:

	return ret;
}

static void irq_unregister_child(struct irq_cascade_desc *cascade, int irq,
				 const void *arg, struct irq_desc *desc)
{
	struct irq_desc *child, *parent = &cascade->desc;
	int hw_irq = irq - cascade->irq_base;
	struct list_item *list, *head = &cascade->child[hw_irq].list;
	unsigned int core = cpu_get_id();

	list_for_item(list, head) {
		child = container_of(list, struct irq_desc, irq_list);

		if (child->handler_arg == arg) {
			list_item_del(&child->irq_list);
			cascade->num_children[core]--;
			if (!desc)
				rfree(child);

			/*
			 * unregister the root interrupt if this l2 is the last
			 * registered child.
			 */
			if (!cascade->num_children[core])
				interrupt_unregister_internal(parent->irq,
							      parent, parent);


			break;
		}

	}

}

static uint32_t irq_enable_child(struct irq_cascade_desc *cascade, int irq,
				 void *arg)
{
	int hw_irq = irq - cascade->irq_base;
	unsigned int core = cpu_get_id();
	struct irq_child *child;
	unsigned int child_idx;
	struct list_item *list;
	unsigned long flags;

	/*
	 * Locking is child to parent: when called recursively we are already
	 * holding the child's lock and then also taking the parent's lock. The
	 * same holds for the interrupt_(un)register() paths.
	 */
	spin_lock_irq(&cascade->lock, flags);

	child = cascade->child + hw_irq;
	child_idx = cascade->global_mask ? 0 : core;

	list_for_item(list, &child->list) {
		struct irq_desc *d = container_of(list,
						  struct irq_desc, irq_list);

		if (d->handler_arg == arg) {
			d->cpu_mask |= 1 << core;
			break;
		}

	}

	if (!child->enable_count[child_idx]++) {
		/* enable the parent interrupt */
		if (!cascade->enable_count[core]++)
			interrupt_enable(cascade->desc.irq,
					 cascade->desc.handler_arg);

		/* enable the child interrupt */
		interrupt_unmask(irq, core);
	}


	spin_unlock_irq(&cascade->lock, flags);

	return 0;
}

static uint32_t irq_disable_child(struct irq_cascade_desc *cascade, int irq,
				  void *arg)
{
	int hw_irq = irq - cascade->irq_base;
	unsigned int core = cpu_get_id();
	struct irq_child *child;
	unsigned int child_idx;
	struct list_item *list;
	unsigned long flags;

	spin_lock_irq(&cascade->lock, flags);

	child = cascade->child + hw_irq;
	child_idx = cascade->global_mask ? 0 : core;

	list_for_item(list, &child->list) {
		struct irq_desc *d = container_of(list,
						  struct irq_desc, irq_list);

		if (d->handler_arg == arg) {
			d->cpu_mask &= ~(1 << core);
			break;
		}

	}

	if (!child->enable_count[child_idx]) {
		tr_err(&irq_tr, "IRQ %x unbalanced interrupt_disable()",
		       irq);
	} else if (!--child->enable_count[child_idx]) {
		/* disable the child interrupt */
		interrupt_mask(irq, core);

		/* disable the parent interrupt */
		if (!--cascade->enable_count[core])
			interrupt_disable(cascade->desc.irq,
					  cascade->desc.handler_arg);
	}


	spin_unlock_irq(&cascade->lock, flags);

	return 0;
}

int interrupt_register(uint32_t irq, void (*handler)(void *arg), void *arg)
{
	return interrupt_register_internal(irq, handler, arg, NULL);
}

static int interrupt_register_internal(uint32_t irq, void (*handler)(void *arg),
				       void *arg, struct irq_desc *desc)
{
	struct irq_cascade_desc *cascade;
	/* Avoid a bogus compiler warning */
	unsigned long flags = 0;
	int ret;

	/* no parent means we are registering DSP internal IRQ */
	cascade = interrupt_get_parent(irq);
	if (!cascade)
		return arch_interrupt_register(irq, handler, arg);

	spin_lock_irq(&cascade->lock, flags);
	ret = irq_register_child(cascade, irq, handler, arg, desc);
	spin_unlock_irq(&cascade->lock, flags);

	return ret;
}

void interrupt_unregister(uint32_t irq, const void *arg)
{
	interrupt_unregister_internal(irq, arg, NULL);
}

static void interrupt_unregister_internal(uint32_t irq, const void *arg,
					  struct irq_desc *desc)
{
	struct irq_cascade_desc *cascade;
	/* Avoid a bogus compiler warning */
	unsigned long flags = 0;

	/* no parent means we are unregistering DSP internal IRQ */
	cascade = interrupt_get_parent(irq);
	if (!cascade) {
		arch_interrupt_unregister(irq);
		return;
	}

	spin_lock_irq(&cascade->lock, flags);
	irq_unregister_child(cascade, irq, arg, desc);
	spin_unlock_irq(&cascade->lock, flags);
}

uint32_t interrupt_enable(uint32_t irq, void *arg)
{
	struct irq_cascade_desc *cascade;

	/* no parent means we are enabling DSP internal IRQ */
	cascade = interrupt_get_parent(irq);
	if (cascade)
		return irq_enable_child(cascade, irq, arg);

	return arch_interrupt_enable_mask(1 << irq);
}

uint32_t interrupt_disable(uint32_t irq, void *arg)
{
	struct irq_cascade_desc *cascade;

	/* no parent means we are disabling DSP internal IRQ */
	cascade = interrupt_get_parent(irq);
	if (cascade)
		return irq_disable_child(cascade, irq, arg);

	return arch_interrupt_disable_mask(1 << irq);
}

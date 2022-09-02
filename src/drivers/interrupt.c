// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Keyon Jie <yang.jie@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <rtos/interrupt.h>
#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(irq, CONFIG_SOF_LOG_LEVEL);

/* 1862d39a-3a84-4d64-8c91-dce1dfc122db */

DECLARE_SOF_UUID("irq", irq_uuid, 0x1862d39a, 0x3a84, 0x4d64,
		 0x8c, 0x91, 0xdc, 0xe1, 0xdf, 0xc1, 0x22, 0xdb);

DECLARE_TR_CTX(irq_tr, SOF_UUID(irq_uuid), LOG_LEVEL_INFO);

/* For i.MX, when building SOF with Zephyr, we use wrapper.c,
 * interrupt.c and interrupt-irqsteer.c which causes name
 * collisions.
 * In order to avoid this and make any second level interrupt
 * handling go through interrupt-irqsteer.c define macros to
 * rename the duplicated functions.
 */
#if defined(__ZEPHYR__) && defined(CONFIG_IMX)
#define interrupt_get_irq mux_interrupt_get_irq
#define interrupt_register mux_interrupt_register
#define interrupt_unregister mux_interrupt_unregister
#define interrupt_enable mux_interrupt_enable
#define interrupt_disable mux_interrupt_disable
#endif

static SHARED_DATA struct cascade_root cascade_root;

static int interrupt_register_internal(uint32_t irq, void (*handler)(void *arg),
				       void *arg, struct irq_desc *desc);
static void interrupt_unregister_internal(uint32_t irq, const void *arg,
					  struct irq_desc *desc);

int interrupt_cascade_register(const struct irq_cascade_tmpl *tmpl)
{
	struct cascade_root *root = cascade_root_get();
	struct irq_cascade_desc **cascade;
	k_spinlock_key_t key;
	unsigned int i;
	int ret;

	if (!tmpl->name || !tmpl->ops)
		return -EINVAL;

	key = k_spin_lock(&root->lock);

	for (cascade = &root->list; *cascade;
	     cascade = &(*cascade)->next) {
		if (!rstrcmp((*cascade)->name, tmpl->name)) {
			ret = -EEXIST;
			tr_err(&irq_tr, "cascading IRQ controller name duplication!");
			goto unlock;
		}

	}

	*cascade = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(**cascade));

	k_spinlock_init(&(*cascade)->lock);

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

	k_spin_unlock(&root->lock, key);

	return ret;
}

int interrupt_get_irq(unsigned int irq, const char *name)
{
	struct cascade_root *root = cascade_root_get();
	struct irq_cascade_desc *cascade;
	int ret = -ENODEV;
	k_spinlock_key_t key;

	if (!name || name[0] == '\0')
		return irq;

	/* If a name is specified, irq must be <= PLATFORM_IRQ_CHILDREN */
	if (irq >= PLATFORM_IRQ_CHILDREN) {
		tr_err(&irq_tr, "IRQ %d invalid as a child interrupt!",
		       irq);
		return -EINVAL;
	}

	key = k_spin_lock(&root->lock);

	for (cascade = root->list; cascade; cascade = cascade->next) {
		/* .name is non-volatile */
		if (!rstrcmp(name, cascade->name)) {
			ret = cascade->irq_base + irq;
			break;
		}

	}


	k_spin_unlock(&root->lock, key);

	return ret;
}

struct irq_cascade_desc *interrupt_get_parent(uint32_t irq)
{
	struct cascade_root *root = cascade_root_get();
	struct irq_cascade_desc *cascade, *c = NULL;
	k_spinlock_key_t key;

	if (irq < PLATFORM_IRQ_HW_NUM)
		return NULL;

	key = k_spin_lock(&root->lock);

	for (cascade = root->list; cascade; cascade = cascade->next) {
		if (irq >= cascade->irq_base &&
		    irq < cascade->irq_base + PLATFORM_IRQ_CHILDREN) {
			c = cascade;
			break;
		}

	}


	k_spin_unlock(&root->lock, key);

	return c;
}

void interrupt_init(struct sof *sof)
{
	sof->cascade_root = platform_shared_get(&cascade_root,
						sizeof(cascade_root));

	sof->cascade_root->last_irq = PLATFORM_IRQ_FIRST_CHILD - 1;
	k_spinlock_init(&sof->cascade_root->lock);
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
	k_spinlock_key_t key;

	/*
	 * Locking is child to parent: when called recursively we are already
	 * holding the child's lock and then also taking the parent's lock. The
	 * same holds for the interrupt_(un)register() paths.
	 */
	key = k_spin_lock(&cascade->lock);

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


	k_spin_unlock(&cascade->lock, key);

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
	k_spinlock_key_t key;

	key = k_spin_lock(&cascade->lock);

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


	k_spin_unlock(&cascade->lock, key);

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
	k_spinlock_key_t key;
	int ret;

	/* no parent means we are registering DSP internal IRQ */
	cascade = interrupt_get_parent(irq);
	if (!cascade) {
#if defined(__ZEPHYR__) && defined(CONFIG_IMX)
/* undefine the macro so that interrupt_register()
 * is resolved to the one from wrapper.c
 */
#undef interrupt_register

		return interrupt_register(irq, handler, arg);
#else
		return arch_interrupt_register(irq, handler, arg);
#endif
	}

	key = k_spin_lock(&cascade->lock);
	ret = irq_register_child(cascade, irq, handler, arg, desc);
	k_spin_unlock(&cascade->lock, key);

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
	k_spinlock_key_t key;

	/* no parent means we are unregistering DSP internal IRQ */
	cascade = interrupt_get_parent(irq);
	if (!cascade) {
#if defined(__ZEPHYR__) && defined(CONFIG_IMX)
/* undefine the macro so that interrupt_unregister()
 * is resolved to the one from wrapper.c
 */
#undef interrupt_unregister

		interrupt_unregister(irq, arg);
#else
		arch_interrupt_unregister(irq);
#endif
		return;
	}

	key = k_spin_lock(&cascade->lock);
	irq_unregister_child(cascade, irq, arg, desc);
	k_spin_unlock(&cascade->lock, key);
}

uint32_t interrupt_enable(uint32_t irq, void *arg)
{
	struct irq_cascade_desc *cascade;

	/* no parent means we are enabling DSP internal IRQ */
	cascade = interrupt_get_parent(irq);
	if (cascade)
		return irq_enable_child(cascade, irq, arg);

#if defined(__ZEPHYR__) && defined(CONFIG_IMX)
/* undefine the macro so that interrupt_enable()
 * is resolved to the one from wrapper.c
 */
#undef interrupt_enable

	return interrupt_enable(irq, arg);
#else
	return arch_interrupt_enable_mask(1 << irq);
#endif
}

uint32_t interrupt_disable(uint32_t irq, void *arg)
{
	struct irq_cascade_desc *cascade;

	/* no parent means we are disabling DSP internal IRQ */
	cascade = interrupt_get_parent(irq);
	if (cascade)
		return irq_disable_child(cascade, irq, arg);

#if defined(__ZEPHYR__) && defined(CONFIG_IMX)
/* undefine the macro so that interrupt_disable()
 * is resolved to the one from wrapper.c
 */
#undef interrupt_disable

	return interrupt_disable(irq, arg);
#else
	return arch_interrupt_disable_mask(1 << irq);
#endif
}

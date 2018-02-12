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

#include <reef/interrupt.h>
#include <reef/interrupt-map.h>
#include <reef/alloc.h>
#include <arch/interrupt.h>
#include <platform/interrupt.h>
#include <stdint.h>
#include <stdlib.h>

int irq_register_child(struct irq_parent *parent, int irq,
	void (*handler)(void *arg), void *arg)
{
	int ret = 0;

	if (parent == NULL)
		return -EINVAL;

	spin_lock(&parent->lock);

	/* does child already exist ? */
	if (parent->child[REEF_IRQ_BIT(irq)]) {
		/* already registered, return */
		goto finish;
	}

	/* init child */
	parent->child[REEF_IRQ_BIT(irq)] =
		rzalloc(RZONE_SYS, RFLAGS_NONE, sizeof(struct irq_child));
	parent->child[REEF_IRQ_BIT(irq)]->enabled = 0;
	parent->child[REEF_IRQ_BIT(irq)]->handler = handler;
	parent->child[REEF_IRQ_BIT(irq)]->handler_arg = arg;

	/* do we need to register parent ? */
	if (parent->num_children == 0) {
		ret = arch_interrupt_register(parent->num,
			parent->handler, parent);
	}

	/* increment number of children */
	parent->num_children += 1;

finish:
	spin_unlock(&parent->lock);
	return ret;

}

void irq_unregister_child(struct irq_parent *parent, int irq)
{
	spin_lock(&parent->lock);

	/* does child already exist ? */
	if (parent->child[REEF_IRQ_BIT(irq)] == NULL)
		goto finish;

	/* free child */
	parent->num_children -= 1;
	rfree(parent->child[REEF_IRQ_BIT(irq)]);
	parent->child[REEF_IRQ_BIT(irq)] = NULL;

	/*
	 * unregister the root interrupt if the this l2 is
	 * the last registered one.
	 */
	if (parent->num_children == 0)
		arch_interrupt_unregister(parent->num);

finish:
	spin_unlock(&parent->lock);
}

uint32_t irq_enable_child(struct irq_parent *parent, int irq)
{
	struct irq_child *child;

	spin_lock(&parent->lock);

	child =parent->child[REEF_IRQ_BIT(irq)];

	/* already enabled ? */
	if (child->enabled)
		goto finish;

	/* enable the parent interrupt */
	if (parent->enabled_count == 0)
		arch_interrupt_enable_mask(1 << REEF_IRQ_NUMBER(irq));
	child->enabled = 1;
	parent->enabled_count++;

	/* enable the child interrupt */
	platform_interrupt_unmask(irq, 0);

finish:
	spin_unlock(&parent->lock);
	return 0;

}

uint32_t irq_disable_child(struct irq_parent *parent, int irq)
{
	struct irq_child *child;

	spin_lock(&parent->lock);

	child =parent->child[REEF_IRQ_BIT(irq)];

	/* already disabled ? */
	if (!child->enabled)
		goto finish;

	/* disable the child interrupt */
	platform_interrupt_mask(irq, 0);
	child->enabled = 0;

	/* disable the parent interrupt */
	parent->enabled_count--;
	if (parent->enabled_count == 0)
		arch_interrupt_disable_mask(1 << REEF_IRQ_NUMBER(irq));

finish:
	spin_unlock(&parent->lock);
	return 0;

}

int interrupt_register(uint32_t irq,
	void (*handler)(void *arg), void *arg)
{
	struct irq_parent *parent;

	/* no parent means we are registering DSP internal IRQ */
	parent = platform_irq_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_register(irq, handler, arg);
	else
		return irq_register_child(parent, irq, handler, arg);
}

void interrupt_unregister(uint32_t irq)
{
	struct irq_parent *parent;

	/* no parent means we are unregistering DSP internal IRQ */
	parent = platform_irq_get_parent(irq);
	if (parent == NULL)
		arch_interrupt_unregister(irq);
	else
		irq_unregister_child(parent, irq);
}

uint32_t interrupt_enable(uint32_t irq)
{
	struct irq_parent *parent;

	/* no parent means we are enabling DSP internal IRQ */
	parent = platform_irq_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_enable_mask(1 << irq);
	else
		return irq_enable_child(parent, irq);
}

uint32_t interrupt_disable(uint32_t irq)
{
	struct irq_parent *parent;

	/* no parent means we are disabling DSP internal IRQ */
	parent = platform_irq_get_parent(irq);
	if (parent == NULL)
		return arch_interrupt_disable_mask(1 << irq);
	else
		return irq_disable_child(parent, irq);
}


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

#include <reef/reef.h>
#include <reef/interrupt.h>
#include <reef/interrupt-map.h>
#include <arch/interrupt.h>
#include <platform/interrupt.h>
#include <platform/shim.h>
#include <stdint.h>
#include <stdlib.h>

static void parent_level2_handler(void *data)
{
	struct irq_parent *parent = (struct irq_parent *)data;
	struct irq_child * child = NULL;
	uint32_t status;
	uint32_t i = 0;

	/* mask the parent IRQ */
	arch_interrupt_disable_mask(1 << IRQ_NUM_EXT_LEVEL2);

	/* mask all child interrupts */
	status = irq_read(REG_IRQ_IL2SD(0));
	irq_write(REG_IRQ_IL2MSD(0), status);

	/* handle each child */
	while (status) {

		/* any IRQ for this child bit ? */
		if ((status & 0x1) == 0)
			goto next;

		/* get child if any and run handler */
		child = parent->child[i];
		if (child && child->handler) {
			child->handler(child->handler_arg);

			/* unmask this bit i interrupt */
			irq_write(REG_IRQ_IL2MCD(0), 0x1 << i);
		} else {
			/* nobody cared ? */
			trace_irq_error("nbc");
		}

next:
		status >>= 1;
		i ++;
	}

	/* clear parent and unmask */
	arch_interrupt_clear(IRQ_NUM_EXT_LEVEL2);
	arch_interrupt_enable_mask(1 << IRQ_NUM_EXT_LEVEL2);
}

static void parent_level3_handler(void *data)
{
	struct irq_parent *parent = (struct irq_parent *)data;
	struct irq_child * child = NULL;
	uint32_t status;
	uint32_t i = 0;

	/* mask the parent IRQ */
	arch_interrupt_disable_mask(1 << IRQ_NUM_EXT_LEVEL3);

	/* mask all child interrupts */
	status = irq_read(REG_IRQ_IL3SD(0));
	irq_write(REG_IRQ_IL3MSD(0), status);

	/* handle each child */
	while (status) {

		/* any IRQ for this child bit ? */
		if ((status & 0x1) == 0)
			goto next;

		/* get child if any and run handler */
		child = parent->child[i];
		if (child && child->handler) {
			child->handler(child->handler_arg);

			/* unmask this bit i interrupt */
			irq_write(REG_IRQ_IL3MCD(0), 0x1 << i);
		} else {
			/* nobody cared ? */
			trace_irq_error("nbc");
		}

next:
		status >>= 1;
		i ++;
	}

	/* clear parent and unmask */
	arch_interrupt_clear(IRQ_NUM_EXT_LEVEL3);
	arch_interrupt_enable_mask(1 << IRQ_NUM_EXT_LEVEL3);
}

static void parent_level4_handler(void *data)
{
	struct irq_parent *parent = (struct irq_parent *)data;
	struct irq_child * child = NULL;
	uint32_t status;
	uint32_t i = 0;

	/* mask the parent IRQ */
	arch_interrupt_disable_mask(1 << IRQ_NUM_EXT_LEVEL4);

	/* mask all child interrupts */
	status = irq_read(REG_IRQ_IL4SD(0));
	irq_write(REG_IRQ_IL4MSD(0), status);

	/* handle each child */
	while (status) {

		/* any IRQ for this child bit ? */
		if ((status & 0x1) == 0)
			goto next;

		/* get child if any and run handler */
		child = parent->child[i];
		if (child && child->handler) {
			child->handler(child->handler_arg);

			/* unmask this bit i interrupt */
			irq_write(REG_IRQ_IL4MCD(0), 0x1 << i);
		} else {
			/* nobody cared ? */
			trace_irq_error("nbc");
		}

next:
		status >>= 1;
		i ++;
	}

	/* clear parent and unmask */
	arch_interrupt_clear(IRQ_NUM_EXT_LEVEL4);
	arch_interrupt_enable_mask(1 << IRQ_NUM_EXT_LEVEL4);
}

static void parent_level5_handler(void *data)
{
	struct irq_parent *parent = (struct irq_parent *)data;
	struct irq_child * child = NULL;
	uint32_t status;
	uint32_t i = 0;

	/* mask the parent IRQ */
	arch_interrupt_disable_mask(1 << IRQ_NUM_EXT_LEVEL5);

	/* mask all child interrupts */
	status = irq_read(REG_IRQ_IL5SD(0));
	irq_write(REG_IRQ_IL5MSD(0), status);

	/* handle each child */
	while (status) {

		/* any IRQ for this child bit ? */
		if ((status & 0x1) == 0)
			goto next;

		/* get child if any and run handler */
		child = parent->child[i];
		if (child && child->handler) {
			child->handler(child->handler_arg);

			/* unmask this bit i interrupt */
			irq_write(REG_IRQ_IL5MCD(0), 0x1 << i);
		} else {
			/* nobody cared ? */
			trace_irq_error("nbc");
		}

next:
		status >>= 1;
		i ++;
	}

	/* clear parent and unmask */
	arch_interrupt_clear(IRQ_NUM_EXT_LEVEL5);
	arch_interrupt_enable_mask(1 << IRQ_NUM_EXT_LEVEL5);
}

/* DSP internal interrupts */
static struct irq_parent dsp_irq[4] = {
	{IRQ_NUM_EXT_LEVEL2, parent_level2_handler, },
	{IRQ_NUM_EXT_LEVEL3, parent_level3_handler, },
	{IRQ_NUM_EXT_LEVEL4, parent_level4_handler, },
	{IRQ_NUM_EXT_LEVEL5, parent_level5_handler, },
};

struct irq_parent *platform_irq_get_parent(uint32_t irq)
{
	switch (REEF_IRQ_NUMBER(irq)) {
	case IRQ_NUM_EXT_LEVEL2:
		return &dsp_irq[0];
	case IRQ_NUM_EXT_LEVEL3:
		return &dsp_irq[1];
	case IRQ_NUM_EXT_LEVEL4:
		return &dsp_irq[2];
	case IRQ_NUM_EXT_LEVEL5:
		return &dsp_irq[3];
	default:
		return NULL;
	}
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void platform_interrupt_mask(uint32_t irq, uint32_t mask)
{
	/* mask external interrupt bit */
	switch (REEF_IRQ_NUMBER(irq)) {
	case IRQ_NUM_EXT_LEVEL5:
		irq_write(REG_IRQ_IL5MSD(0), 1 << REEF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL4:
		irq_write(REG_IRQ_IL4MSD(0), 1 << REEF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL3:
		irq_write(REG_IRQ_IL3MSD(0), 1 << REEF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL2:
		irq_write(REG_IRQ_IL2MSD(0), 1 << REEF_IRQ_BIT(irq));
		break;
	default:
		break;
	}

}

void platform_interrupt_unmask(uint32_t irq, uint32_t mask)
{
	/* unmask external interrupt bit */
	switch (REEF_IRQ_NUMBER(irq)) {
	case IRQ_NUM_EXT_LEVEL5:
		irq_write(REG_IRQ_IL5MCD(0), 1 << REEF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL4:
		irq_write(REG_IRQ_IL4MCD(0), 1 << REEF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL3:
		irq_write(REG_IRQ_IL3MCD(0), 1 << REEF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL2:
		irq_write(REG_IRQ_IL2MCD(0), 1 << REEF_IRQ_BIT(irq));
		break;
	default:
		break;
	}

}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
}

void platform_interrupt_init(void)
{
	int i;

	/* mask all external IRQs by default */
	irq_write(REG_IRQ_IL2MSD(0), REG_IRQ_IL2MD_ALL);
	irq_write(REG_IRQ_IL3MSD(0), REG_IRQ_IL3MD_ALL);
	irq_write(REG_IRQ_IL4MSD(0), REG_IRQ_IL4MD_ALL);
	irq_write(REG_IRQ_IL5MSD(0), REG_IRQ_IL5MD_ALL);

	for (i = 0; i < ARRAY_SIZE(dsp_irq); i++) {
		spinlock_init(&dsp_irq[i].lock);
	}
}


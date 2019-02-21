/*
 * Copyright (c) 2018, Intel Corporation
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
 *         Rander Wang <rander.wang@intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 *
 */

#include <sof/sof.h>
#include <sof/interrupt.h>
#include <sof/interrupt-map.h>
#include <sof/cpu.h>
#include <arch/interrupt.h>
#include <platform/interrupt.h>
#include <platform/platform.h>
#include <platform/shim.h>
#include <stdint.h>
#include <stdlib.h>
#include <cavs/version.h>

/*
 * Number of status reload tries before warning the user we are in an IRQ
 * storm where some device(s) are repeatedly interrupting and cannot be
 * cleared.
 */
#define LVL2_MAX_TRIES		1000

char irq_name_level2[] = "level2";
char irq_name_level3[] = "level3";
char irq_name_level4[] = "level4";
char irq_name_level5[] = "level5";

/*
 * The level2 handler attempts to try and fairly service interrupt sources by
 * servicing on first come first served basis. If two or more IRQs arrive at the
 * same time then they are serviced in order of ascending status bit.
 */
static inline void irq_lvl2_handler(void *data, int level, uint32_t ilxsd,
				    uint32_t ilxmsd, uint32_t ilxmcd)
{
	struct irq_desc *parent = (struct irq_desc *)data;
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);
	struct irq_desc *child = NULL;
	struct list_item *clist;
	uint32_t status;
	uint32_t i = 0;
	uint32_t tries = LVL2_MAX_TRIES;

	/* read active interrupt status */
	status = irq_read(ilxsd);

	/* handle each child */
	while (irq_read(ilxsd)) {

		/* are all IRQs serviced from last status ? */
		if (status == 0x0) {
			/* yes, so reload the new status and service again */
			status = irq_read(ilxsd);
			i = 0;
			tries--;
		}

		/* any devices continually interrupting / can't be cleared ? */
		if (!tries) {
			tries = LVL2_MAX_TRIES;
			trace_irq_error("irq_lvl2_handler() error: "
					"IRQ storm at level %d status %08X",
					level, irq_read(ilxsd));
		}

		/* any IRQ for this child bit ? */
		if ((status & 0x1) == 0)
			goto next;

		/* get child if any and run handler */
		list_for_item(clist, &cascade->child[i]) {
			child = container_of(clist, struct irq_desc, irq_list);

			if (child && child->handler) {
				child->handler(child->handler_arg);
			} else {
				/* nobody cared ? */
				trace_irq_error("irq_lvl2_handler() error: "
						"nobody cared level %d bit %d",
						level, i);
				/* now mask it */
				irq_write(ilxmcd, 0x1 << i);
			}
		}

next:
		status >>= 1;
		i++;
	}
}

#define IRQ_LVL2_HANDLER(n) int core = cpu_get_id(); \
				irq_lvl2_handler(data, \
				 IRQ_NUM_EXT_LEVEL##n, \
				 REG_IRQ_IL##n##SD(core), \
				 REG_IRQ_IL##n##MSD(core), \
				 REG_IRQ_IL##n##MCD(core))

static void irq_lvl2_level2_handler(void *data)
{
	IRQ_LVL2_HANDLER(2);
}

static void irq_lvl2_level3_handler(void *data)
{
	IRQ_LVL2_HANDLER(3);
}

static void irq_lvl2_level4_handler(void *data)
{
	IRQ_LVL2_HANDLER(4);
}

static void irq_lvl2_level5_handler(void *data)
{
	IRQ_LVL2_HANDLER(5);
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void interrupt_mask(uint32_t irq)
{
	struct irq_desc *parent = interrupt_get_parent(irq);
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);
	if (parent && cascade->ops->mask)
		cascade->ops->mask(parent, irq);
}

void interrupt_unmask(uint32_t irq)
{
	struct irq_desc *parent = interrupt_get_parent(irq);
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);
	if (parent && cascade->ops->unmask)
		cascade->ops->unmask(parent, irq);
}

static void irq_mask(struct irq_desc *desc, uint32_t irq)
{
	int core = SOF_IRQ_CPU(irq);

	/* mask external interrupt bit */
	switch (desc->irq) {
	case IRQ_NUM_EXT_LEVEL5:
		irq_write(REG_IRQ_IL5MSD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL4:
		irq_write(REG_IRQ_IL4MSD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL3:
		irq_write(REG_IRQ_IL3MSD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL2:
		irq_write(REG_IRQ_IL2MSD(core), 1 << SOF_IRQ_BIT(irq));
	}
}

static void irq_unmask(struct irq_desc *desc, uint32_t irq)
{
	int core = SOF_IRQ_CPU(irq);

	/* unmask external interrupt bit */
	switch (desc->irq) {
	case IRQ_NUM_EXT_LEVEL5:
		irq_write(REG_IRQ_IL5MCD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL4:
		irq_write(REG_IRQ_IL4MCD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL3:
		irq_write(REG_IRQ_IL3MCD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL2:
		irq_write(REG_IRQ_IL2MCD(core), 1 << SOF_IRQ_BIT(irq));
	}
}

static const struct irq_cascade_ops irq_ops = {
	.mask = irq_mask,
	.unmask = irq_unmask,
};

/* DSP internal interrupts */
static struct irq_cascade_tmpl dsp_irq[4] = {
	{
		.name = irq_name_level2,
		.irq = IRQ_NUM_EXT_LEVEL2,
		.handler = irq_lvl2_level2_handler,
		.ops = &irq_ops,
	}, {
		.name = irq_name_level3,
		.irq = IRQ_NUM_EXT_LEVEL3,
		.handler = irq_lvl2_level3_handler,
		.ops = &irq_ops,
	}, {
		.name = irq_name_level4,
		.irq = IRQ_NUM_EXT_LEVEL4,
		.handler = irq_lvl2_level4_handler,
		.ops = &irq_ops,
	}, {
		.name = irq_name_level5,
		.irq = IRQ_NUM_EXT_LEVEL5,
		.handler = irq_lvl2_level5_handler,
		.ops = &irq_ops,
	},
};

void platform_interrupt_set(uint32_t irq)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_set(SOF_IRQ_NUMBER(irq));
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_clear(SOF_IRQ_NUMBER(irq));
}

/* Called on each core: from platform_init() and from slave_core_init() */
void platform_interrupt_init(void)
{
	int i;
	int core = cpu_get_id();

	/* mask all external IRQs by default */
	irq_write(REG_IRQ_IL2MSD(core), REG_IRQ_IL2MD_ALL);
	irq_write(REG_IRQ_IL3MSD(core), REG_IRQ_IL3MD_ALL);
	irq_write(REG_IRQ_IL4MSD(core), REG_IRQ_IL4MD_ALL);
	irq_write(REG_IRQ_IL5MSD(core), REG_IRQ_IL5MD_ALL);

	if (core != PLATFORM_MASTER_CORE_ID)
		return;

	for (i = 0; i < ARRAY_SIZE(dsp_irq); i++)
		interrupt_cascade_register(dsp_irq + i);
}

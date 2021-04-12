// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Keyon Jie <yang.jie@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/spinlock.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* f6448dbf-a8ec-4660-ada2-08a0011a7a86 */
DECLARE_SOF_UUID("irq-cavs", irq_cavs_uuid, 0xf6448dbf, 0xa8ec, 0x4660,
		 0xad, 0xa2, 0x08, 0xa0, 0x01, 0x1a, 0x7a, 0x86);

DECLARE_TR_CTX(irq_c_tr, SOF_UUID(irq_cavs_uuid), LOG_LEVEL_INFO);

/*
 * Number of status reload tries before warning the user we are in an IRQ
 * storm where some device(s) are repeatedly interrupting and cannot be
 * cleared.
 */
#define LVL2_MAX_TRIES		1000

#if CONFIG_INTERRUPT_LEVEL_2
const char irq_name_level2[] = "level2";
#endif
#if CONFIG_INTERRUPT_LEVEL_3
const char irq_name_level3[] = "level3";
#endif
#if CONFIG_INTERRUPT_LEVEL_4
const char irq_name_level4[] = "level4";
#endif
#if CONFIG_INTERRUPT_LEVEL_5
const char irq_name_level5[] = "level5";
#endif

/*
 * The level2 handler attempts to try and fairly service interrupt sources by
 * servicing on first come first served basis. If two or more IRQs arrive at the
 * same time then they are serviced in order of ascending status bit.
 */
static inline void irq_lvl2_handler(void *data, int level, uint32_t ilxsd,
				    uint32_t ilxmsd)
{
	struct irq_desc *parent = data;
	struct irq_cascade_desc *cascade = container_of(parent,
						struct irq_cascade_desc, desc);
	struct irq_desc *child = NULL;
	int core = cpu_get_id();
	struct list_item *clist;
	uint32_t status;
	uint32_t tries = LVL2_MAX_TRIES;

	/* read active interrupt status */
	status = irq_read(ilxsd);
	if (!status)
		return;

	/* handle each child */
	for (;;) {
		unsigned int bit = ffs(status) - 1;
		bool handled = false;

		status &= ~(1 << bit);

		spin_lock(&cascade->lock);

		/* get child if any and run handler */
		list_for_item(clist, &cascade->child[bit].list) {
			child = container_of(clist, struct irq_desc, irq_list);

			if (child->handler && (child->cpu_mask & 1 << core)) {
				/* run handler in non atomic context */
				spin_unlock(&cascade->lock);
				child->handler(child->handler_arg);
				spin_lock(&cascade->lock);

				handled = true;
			}

		}

		spin_unlock(&cascade->lock);

		if (!handled) {
			/* nobody cared ? */
			tr_err(&irq_c_tr, "irq_lvl2_handler(): nobody cared level %d bit %d",
			       level, bit);
			/* now mask it */
			irq_write(ilxmsd, 0x1 << bit);
		}

		/* are all IRQs serviced from last status ? */
		if (status)
			continue;

		/* yes, so reload the new status and service again */
		status = irq_read(ilxsd);
		if (!status)
			break;

		/* any devices continually interrupting / can't be cleared ? */
		if (!--tries) {
			tries = LVL2_MAX_TRIES;
			tr_err(&irq_c_tr, "irq_lvl2_handler(): IRQ storm at level %d status %08X",
			       level, irq_read(ilxsd));
		}
	}
}

#define IRQ_LVL2_HANDLER(n) int core = cpu_get_id(); \
				irq_lvl2_handler(data, \
				 IRQ_NUM_EXT_LEVEL##n, \
				 REG_IRQ_IL##n##SD(core), \
				 REG_IRQ_IL##n##MSD(core))

#if CONFIG_INTERRUPT_LEVEL_2
static void irq_lvl2_level2_handler(void *data)
{
	IRQ_LVL2_HANDLER(2);
}
#endif

#if CONFIG_INTERRUPT_LEVEL_3
static void irq_lvl2_level3_handler(void *data)
{
	IRQ_LVL2_HANDLER(3);
}
#endif

#if CONFIG_INTERRUPT_LEVEL_4
static void irq_lvl2_level4_handler(void *data)
{
	IRQ_LVL2_HANDLER(4);
}
#endif

#if CONFIG_INTERRUPT_LEVEL_5
static void irq_lvl2_level5_handler(void *data)
{
	IRQ_LVL2_HANDLER(5);
}
#endif

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
	struct irq_cascade_desc *cascade = interrupt_get_parent(irq);

	if (cascade && cascade->ops->mask)
		cascade->ops->mask(&cascade->desc, irq - cascade->irq_base,
				   cpu);

}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
	struct irq_cascade_desc *cascade = interrupt_get_parent(irq);

	if (cascade && cascade->ops->unmask)
		cascade->ops->unmask(&cascade->desc, irq - cascade->irq_base,
				     cpu);

}

static void irq_mask(struct irq_desc *desc, uint32_t irq, unsigned int core)
{
	/* mask external interrupt bit */
	switch (desc->irq) {
#if CONFIG_INTERRUPT_LEVEL_5
	case IRQ_NUM_EXT_LEVEL5:
		irq_write(REG_IRQ_IL5MSD(core), 1 << irq);
		break;
#endif
#if CONFIG_INTERRUPT_LEVEL_4
	case IRQ_NUM_EXT_LEVEL4:
		irq_write(REG_IRQ_IL4MSD(core), 1 << irq);
		break;
#endif
#if CONFIG_INTERRUPT_LEVEL_3
	case IRQ_NUM_EXT_LEVEL3:
		irq_write(REG_IRQ_IL3MSD(core), 1 << irq);
		break;
#endif
#if CONFIG_INTERRUPT_LEVEL_2
	case IRQ_NUM_EXT_LEVEL2:
		irq_write(REG_IRQ_IL2MSD(core), 1 << irq);
		break;
#endif
	}

}

static void irq_unmask(struct irq_desc *desc, uint32_t irq, unsigned int core)
{
	/* unmask external interrupt bit */
	switch (desc->irq) {
#if CONFIG_INTERRUPT_LEVEL_5
	case IRQ_NUM_EXT_LEVEL5:
		irq_write(REG_IRQ_IL5MCD(core), 1 << irq);
		break;
#endif
#if CONFIG_INTERRUPT_LEVEL_4
	case IRQ_NUM_EXT_LEVEL4:
		irq_write(REG_IRQ_IL4MCD(core), 1 << irq);
		break;
#endif
#if CONFIG_INTERRUPT_LEVEL_3
	case IRQ_NUM_EXT_LEVEL3:
		irq_write(REG_IRQ_IL3MCD(core), 1 << irq);
		break;
#endif
#if CONFIG_INTERRUPT_LEVEL_2
	case IRQ_NUM_EXT_LEVEL2:
		irq_write(REG_IRQ_IL2MCD(core), 1 << irq);
		break;
#endif
	}

}

static const struct irq_cascade_ops irq_ops = {
	.mask = irq_mask,
	.unmask = irq_unmask,
};

/* DSP internal interrupts */
static const struct irq_cascade_tmpl dsp_irq[] = {
#if CONFIG_INTERRUPT_LEVEL_2
	{
		.name = irq_name_level2,
		.irq = IRQ_NUM_EXT_LEVEL2,
		.handler = irq_lvl2_level2_handler,
		.ops = &irq_ops,
		.global_mask = false,
	},
#endif
#if CONFIG_INTERRUPT_LEVEL_3
	{
		.name = irq_name_level3,
		.irq = IRQ_NUM_EXT_LEVEL3,
		.handler = irq_lvl2_level3_handler,
		.ops = &irq_ops,
		.global_mask = false,
	},
#endif
#if CONFIG_INTERRUPT_LEVEL_4
	{
		.name = irq_name_level4,
		.irq = IRQ_NUM_EXT_LEVEL4,
		.handler = irq_lvl2_level4_handler,
		.ops = &irq_ops,
		.global_mask = false,
	},
#endif
#if CONFIG_INTERRUPT_LEVEL_5
	{
		.name = irq_name_level5,
		.irq = IRQ_NUM_EXT_LEVEL5,
		.handler = irq_lvl2_level5_handler,
		.ops = &irq_ops,
		.global_mask = false,
	},
#endif
};

void platform_interrupt_set(uint32_t irq)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_set(irq);
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_clear(irq);
}

/* Called on each core: from platform_init() and from secondary_core_init() */
void platform_interrupt_init(void)
{
	int i;
	int core = cpu_get_id();

	/* mask all external IRQs by default */
	irq_write(REG_IRQ_IL2MSD(core), REG_IRQ_IL2MD_ALL);
	irq_write(REG_IRQ_IL3MSD(core), REG_IRQ_IL3MD_ALL);
	irq_write(REG_IRQ_IL4MSD(core), REG_IRQ_IL4MD_ALL);
	irq_write(REG_IRQ_IL5MSD(core), REG_IRQ_IL5MD_ALL);

	if (core != PLATFORM_PRIMARY_CORE_ID)
		return;

	for (i = 0; i < ARRAY_SIZE(dsp_irq); i++)
		interrupt_cascade_register(dsp_irq + i);
}

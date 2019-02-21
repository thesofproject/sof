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
#include <sof/drivers/interrupt-map.h>
#include <sof/lib/cpu.h>
#include <sof/list.h>
#include <sof/spinlock.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Number of status reload tries before warning the user we are in an IRQ
 * storm where some device(s) are repeatedly interrupting and cannot be
 * cleared.
 */
#define LVL2_MAX_TRIES		1000

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

/* DSP internal interrupts */
static struct irq_cascade_desc dsp_irq[PLATFORM_CORE_COUNT][4] = {
	{{.desc = {IRQ_NUM_EXT_LEVEL2, irq_lvl2_level2_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL3, irq_lvl2_level3_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL4, irq_lvl2_level4_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL5, irq_lvl2_level5_handler, },} },
#if PLATFORM_CORE_COUNT > 1
	{{.desc = {IRQ_NUM_EXT_LEVEL2, irq_lvl2_level2_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL3, irq_lvl2_level3_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL4, irq_lvl2_level4_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL5, irq_lvl2_level5_handler, },} },
#endif
#if PLATFORM_CORE_COUNT > 2
	{{.desc = {IRQ_NUM_EXT_LEVEL2, irq_lvl2_level2_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL3, irq_lvl2_level3_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL4, irq_lvl2_level4_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL5, irq_lvl2_level5_handler, },} },
#endif
#if PLATFORM_CORE_COUNT > 3
	{{.desc = {IRQ_NUM_EXT_LEVEL2, irq_lvl2_level2_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL3, irq_lvl2_level3_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL4, irq_lvl2_level4_handler, },},
	 {.desc = {IRQ_NUM_EXT_LEVEL5, irq_lvl2_level5_handler, },} },
#endif
};

struct irq_desc *platform_irq_get_parent(uint32_t irq)
{
	int core = SOF_IRQ_CPU(irq);

	switch (SOF_IRQ_NUMBER(irq)) {
	case IRQ_NUM_EXT_LEVEL2:
		return &dsp_irq[core][0].desc;
	case IRQ_NUM_EXT_LEVEL3:
		return &dsp_irq[core][1].desc;
	case IRQ_NUM_EXT_LEVEL4:
		return &dsp_irq[core][2].desc;
	case IRQ_NUM_EXT_LEVEL5:
		return &dsp_irq[core][3].desc;
	default:
		return NULL;
	}
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void platform_interrupt_mask(uint32_t irq)
{
	int core = SOF_IRQ_CPU(irq);

	/* mask external interrupt bit */
	switch (SOF_IRQ_NUMBER(irq)) {
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
		break;
	default:
		break;
	}
}

void platform_interrupt_unmask(uint32_t irq)
{
	int core = SOF_IRQ_CPU(irq);

	/* unmask external interrupt bit */
	switch (SOF_IRQ_NUMBER(irq)) {
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
		break;
	default:
		break;
	}
}

void platform_interrupt_set(uint32_t irq)
{
	if (!platform_irq_get_parent(irq))
		arch_interrupt_set(SOF_IRQ_NUMBER(irq));
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	if (!platform_irq_get_parent(irq))
		arch_interrupt_clear(SOF_IRQ_NUMBER(irq));
}

void platform_interrupt_init(void)
{
	int i, j;
	int core = cpu_get_id();

	/* mask all external IRQs by default */
	irq_write(REG_IRQ_IL2MSD(core), REG_IRQ_IL2MD_ALL);
	irq_write(REG_IRQ_IL3MSD(core), REG_IRQ_IL3MD_ALL);
	irq_write(REG_IRQ_IL4MSD(core), REG_IRQ_IL4MD_ALL);
	irq_write(REG_IRQ_IL5MSD(core), REG_IRQ_IL5MD_ALL);

	for (i = 0; i < ARRAY_SIZE(dsp_irq[core]); i++) {
		spinlock_init(&dsp_irq[core][i].lock);
		for (j = 0; j < PLATFORM_IRQ_CHILDREN; j++)
			list_init(&dsp_irq[core][i].child[j]);
	}
}

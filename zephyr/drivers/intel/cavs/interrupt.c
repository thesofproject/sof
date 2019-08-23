// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Keyon Jie <yang.jie@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

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

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void platform_interrupt_mask(uint32_t irq, uint32_t mask)
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

void platform_interrupt_unmask(uint32_t irq, uint32_t mask)
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

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
}

void platform_interrupt_init(void)
{
	int core = cpu_get_id();

	/* mask all external IRQs by default */
	irq_write(REG_IRQ_IL2MSD(core), REG_IRQ_IL2MD_ALL);
	irq_write(REG_IRQ_IL3MSD(core), REG_IRQ_IL3MD_ALL);
	irq_write(REG_IRQ_IL4MSD(core), REG_IRQ_IL4MD_ALL);
	irq_write(REG_IRQ_IL5MSD(core), REG_IRQ_IL5MD_ALL);
}

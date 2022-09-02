// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <rtos/interrupt.h>
#include <sof/lib/shim.h>

#include <stdint.h>

void platform_interrupt_init(void) {}

void platform_interrupt_set(uint32_t irq)
{
	arch_interrupt_set(irq);
}

/* clear mask in PISR, bits are W1C in docs but some bits need preserved ?? */
void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	switch (irq) {
#if CONFIG_XT_INTERRUPT_LEVEL_1
	case IRQ_NUM_SOFTWARE2:
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_2
	case IRQ_NUM_SOFTWARE3:
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_3
	case IRQ_NUM_SOFTWARE4:
	case IRQ_NUM_SOFTWARE5:
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_4
	case IRQ_NUM_EXT_PMC:
	case IRQ_NUM_EXT_IA:
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_1 || CONFIG_XT_INTERRUPT_LEVEL_2 || \
	CONFIG_XT_INTERRUPT_LEVEL_3 || CONFIG_XT_INTERRUPT_LEVEL_4
		arch_interrupt_clear(irq);
		break;
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_5
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PISR, mask << 3);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PISR, mask << 4);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PISR, mask << 5);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PISR, mask << 16);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PISR, mask << 24);
		arch_interrupt_clear(irq);
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PISRH, mask << 0);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PISRH, mask << 8);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PISRH, mask << 9);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PISRH, mask << 10);
		arch_interrupt_clear(irq);
		break;
#endif
#endif
	default:
		break;
	}
}

/* TODO: expand this to 64 bit - should we just return mask of IRQ numbers */
uint32_t platform_interrupt_get_enabled(void)
{
	return shim_read(SHIM_PIMR);
}

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
#if CONFIG_XT_INTERRUPT_LEVEL_5
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PIMR, BIT(3));
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PIMR, BIT(4));
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PIMR, BIT(5));
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PIMR, BIT(16));
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PIMR, BIT(24));
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PIMRH, BIT(8));
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PIMRH, BIT(0));
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PIMRH, BIT(1));
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PIMRH, BIT(2));
		break;
#endif
	default:
		break;
	}
#endif
}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
#if CONFIG_XT_INTERRUPT_LEVEL_5
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(1 << 3));
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(1 << 4));
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(1 << 5));
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(1 << 16));
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(1 << 24));
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(1 << 8));
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(1 << 0));
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(1 << 1));
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(1 << 2));
		break;
#endif
	default:
		break;
	}
#endif
}

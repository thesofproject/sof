// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/drivers/interrupt.h>
#include <sof/lib/shim.h>
#include <stddef.h>
#include <stdint.h>

void platform_interrupt_init(void) {}

struct irq_desc *platform_irq_get_parent(uint32_t irq)
{
	return NULL;
}

void platform_interrupt_set(int irq)
{
	arch_interrupt_set(irq);
}

/* clear mask in PISR, bits are W1C in docs but some bits need preserved ?? */
void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PISR, mask << 3);
		interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PISR, mask << 4);
		interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PISR, mask << 5);
		interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PISR, mask << 16);
		interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PISR, mask << 24);
		interrupt_clear(irq);
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PISRH, mask << 0);
		interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PISRH, mask << 8);
		interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PISRH, mask << 9);
		interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PISRH, mask << 10);
		interrupt_clear(irq);
		break;
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

void platform_interrupt_mask(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PIMR, mask << 3);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PIMR, mask << 4);
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PIMR, mask << 5);
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PIMR, mask << 16);
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PIMR, mask << 24);
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PIMRH, mask << 8);
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PIMRH, mask << 0);
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PIMRH, mask << 1);
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PIMRH, mask << 2);
		break;
#endif
	default:
		break;
	}
}

void platform_interrupt_unmask(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 3));
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 4));
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 5));
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 16));
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 24));
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(mask << 8));
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(mask << 0));
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(mask << 1));
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(mask << 2));
		break;
#endif
	default:
		break;
	}
}

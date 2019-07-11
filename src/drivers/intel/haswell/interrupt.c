// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/sof.h>
#include <sof/drivers/interrupt.h>
#include <sof/shim.h>

void platform_interrupt_init(void) {}

/* haswell does not support child IRQs */
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
	case IRQ_NUM_EXT_DMAC0:
	case IRQ_NUM_EXT_DMAC1:
	case IRQ_NUM_EXT_SSP0:
	case IRQ_NUM_EXT_SSP1:
		interrupt_clear(irq);
		break;
	default:
		break;
	}
}

/* TODO: expand this to 64 bit - should we just return mask of IRQ numbers */
uint32_t platform_interrupt_get_enabled(void)
{
	return shim_read(SHIM_IMRD);
}

void platform_interrupt_mask(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_IMRD, SHIM_IMRD_SSP0);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_IMRD, SHIM_IMRD_SSP1);
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_IMRD, SHIM_IMRD_DMAC0);
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_IMRD, SHIM_IMRD_DMAC1);
		break;
	default:
		break;
	}
}

void platform_interrupt_unmask(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_SSP0);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_SSP1);
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DMAC0);
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DMAC1);
		break;
	default:
		break;
	}
}

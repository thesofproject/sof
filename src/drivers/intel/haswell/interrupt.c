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
	case IRQ_NUM_EXT_SSP0:
	case IRQ_NUM_EXT_SSP1:
	case IRQ_NUM_EXT_IA:
	case IRQ_NUM_SOFTWARE1:
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_2
	case IRQ_NUM_EXT_DMAC0:
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_3
	case IRQ_NUM_EXT_DMAC1:
	case IRQ_NUM_SOFTWARE2:
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_1 || CONFIG_XT_INTERRUPT_LEVEL_2 || \
	CONFIG_XT_INTERRUPT_LEVEL_3
		arch_interrupt_clear(irq);
		break;
#endif
	default:
		break;
	}
}

/* TODO: expand this to 64 bit - should we just return mask of IRQ numbers */
uint32_t platform_interrupt_get_enabled(void)
{
	return shim_read(SHIM_IMRD);
}

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
	switch (irq) {
#if CONFIG_XT_INTERRUPT_LEVEL_1
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_IMRD, SHIM_IMRD_SSP0);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_IMRD, SHIM_IMRD_SSP1);
		break;
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_2
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_IMRD, SHIM_IMRD_DMAC0);
		break;
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_3
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_IMRD, SHIM_IMRD_DMAC1);
		break;
#endif
	default:
		break;
	}
}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
	switch (irq) {
#if CONFIG_XT_INTERRUPT_LEVEL_1
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_SSP0);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_SSP1);
		break;
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_2
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DMAC0);
		break;
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_3
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DMAC1);
		break;
#endif
	default:
		break;
	}
}

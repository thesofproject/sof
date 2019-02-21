// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/drivers/interrupt.h>
#include <stdint.h>

void platform_interrupt_init(void) {}

void platform_interrupt_set(uint32_t irq)
{
	arch_interrupt_set(irq);
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	arch_interrupt_clear(irq);
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void interrupt_mask(uint32_t irq) {}
void interrupt_unmask(uint32_t irq) {}

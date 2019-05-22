// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/sof.h>
#include <sof/interrupt.h>

void platform_interrupt_init(void) {}

struct irq_desc *platform_irq_get_parent(uint32_t irq)
{
	return NULL;
}

void platform_interrupt_set(int irq)
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

void platform_interrupt_mask(uint32_t irq, uint32_t mask) {}
void platform_interrupt_unmask(uint32_t irq, uint32_t mask) {}

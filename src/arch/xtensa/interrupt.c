/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Xtensa architecture initialisation.
 */

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <platform/memory.h>
#include <reef/mailbox.h>
#include <reef/debug.h>
#include <reef/init.h>
#include <reef/interrupt.h>
#include <stdint.h>
#include <stdlib.h>

static uint32_t _mask = 0, _enable = 0;

int interrupt_register(int irq, void(*handler)(void *arg), void *arg)
{
	xthal_set_intclear(0x1 << irq);
	_xtos_set_interrupt_handler_arg(irq, handler, arg);

	_mask |= (0x1 << irq);

	return 0;
}

void interrupt_unregister(int irq)
{
	_xtos_set_interrupt_handler_arg(irq, NULL, NULL);
	_mask &= ~(0x1 << irq);
}

void interrupt_enable(int irq)
{
	_enable |= (0x1 << irq);
	xthal_set_intenable(_enable);
}

void interrupt_enable_sync(void)
{
	xthal_set_intenable(_enable);
}

void interrupt_disable(int irq)
{
	_enable &= ~(0x1 << irq);
	xthal_set_intenable(_enable);
}

void interrupt_set(int irq)
{
	xthal_set_intset(0x1 << irq);
}

uint32_t interrupt_get(void)
{
	return xthal_get_intenable();
}

void interrupt_clear(int irq)
{
	xthal_set_intclear(0x1 << irq);
}

void interrupt_global_disable(uint32_t flags)
{
	flags = xthal_get_intenable();
	xthal_set_intenable(0);
}

void interrupt_global_enable(uint32_t flags)
{
	xthal_set_intenable(flags);
}

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

#ifndef __ARCH_INTERRUPT_H
#define __ARCH_INTERRUPT_H

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <stdint.h>
#include <stdlib.h>

extern uint32_t _arch_irq_enable;

static inline int arch_interrupt_register(int irq,
	void(*handler)(void *arg), void *arg)
{
	xthal_set_intclear(0x1 << irq);
	_xtos_set_interrupt_handler_arg(irq, handler, arg);
	return 0;
}

static inline void arch_interrupt_unregister(int irq)
{
	_xtos_set_interrupt_handler_arg(irq, NULL, NULL);
}

static inline void arch_interrupt_enable(int irq)
{
	_arch_irq_enable |= (0x1 << irq);
	xthal_set_intenable(_arch_irq_enable);
}

static inline void arch_interrupt_enable_sync(void)
{
	xthal_set_intenable(_arch_irq_enable);
}

static inline void arch_interrupt_disable(int irq)
{
	_arch_irq_enable &= ~(0x1 << irq);
	xthal_set_intenable(_arch_irq_enable);
}

static inline uint32_t arch_interrupt_enable_mask(uint32_t mask)
{
	uint32_t old_mask = xthal_get_intenable();

	_arch_irq_enable = mask;
	xthal_set_intenable(_arch_irq_enable);

	return old_mask;
}

static inline void arch_interrupt_set(int irq)
{
	xthal_set_intset(0x1 << irq);
}

static inline uint32_t arch_interrupt_get_enabled(void)
{
	return xthal_get_intenable();
}

static inline uint32_t arch_interrupt_get_status(void)
{
	return xthal_get_interrupt();
}

static inline void arch_interrupt_clear(int irq)
{
	xthal_set_intclear(0x1 << irq);
}

static inline uint32_t arch_interrupt_global_disable(void)
{
	uint32_t flags;

	flags = xthal_get_intenable();
	xthal_set_intenable(0);

	return flags;
}

static inline void arch_interrupt_global_enable(uint32_t flags)
{
	xthal_set_intenable(flags);
}

static inline uint32_t arch_interrupt_local_disable(void)
{
	return 0;
}

static inline void arch_interrupt_local_enable(uint32_t flags)
{
}

#endif

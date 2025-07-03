/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __ARCH_DRIVERS_INTERRUPT_H__
#define __ARCH_DRIVERS_INTERRUPT_H__

#include <xtensa/hal.h>
#include <xtensa/xtruntime.h>
#include <stddef.h>
#include <stdint.h>


static inline int arch_interrupt_register(int irq,
	void (*handler)(void *arg), void *arg)
{
	xthal_set_intclear(0x1 << irq);
	_xtos_set_interrupt_handler_arg(irq, handler, arg);
	return 0;
}

static inline void arch_interrupt_unregister(int irq)
{
	_xtos_set_interrupt_handler_arg(irq, NULL, NULL);
}

/* returns previous mask */
#define arch_interrupt_enable_mask(mask) \
	_xtos_ints_on(mask)

/* returns previous mask */
#define arch_interrupt_disable_mask(mask) \
	_xtos_ints_off(mask)

static inline uint32_t arch_interrupt_get_level(void)
{
	uint32_t level;

	__asm__ __volatile__(
		"       rsr.ps %0\n"
		"       extui  %0, %0, 0, 4\n"
		: "=&a" (level) :: "memory");

	return level;
}

static inline void arch_interrupt_set(int irq)
{
	xthal_set_intset(0x1 << irq);
}

static inline void arch_interrupt_clear(int irq)
{
	xthal_set_intclear(0x1 << irq);
}

static inline uint32_t arch_interrupt_get_enabled(void)
{
	return xthal_get_intenable();
}

static inline uint32_t arch_interrupt_get_status(void)
{
	return xthal_get_interrupt();
}

static inline uint32_t arch_interrupt_global_disable(void)
{
	uint32_t flags;

	__asm__ __volatile__("rsil	%0, 5"
		     : "=a" (flags) :: "memory");
	return flags;
}

static inline void arch_interrupt_global_enable(uint32_t flags)
{
	__asm__ __volatile__("wsr %0, ps; rsync"
		     :: "a" (flags) : "memory");
}

#if CONFIG_WAKEUP_HOOK
void arch_interrupt_on_wakeup(void);
#endif

#endif /* __ARCH_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_INTERRUPT_H__ */

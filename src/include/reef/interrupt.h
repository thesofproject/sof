/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_INTERRUPT__
#define __INCLUDE_INTERRUPT__

#include <stdint.h>
#include <arch/interrupt.h>
#include <reef/trace.h>
#include <reef/debug.h>

#define trace_irq(__e)	trace_event(TRACE_CLASS_IRQ | __e)

static inline int interrupt_register(int irq,
	void(*handler)(void *arg), void *arg)
{
	return arch_interrupt_register(irq, handler, arg);
}

static inline void interrupt_unregister(int irq)
{
	arch_interrupt_unregister(irq);
}

static inline void interrupt_enable(int irq)
{
	arch_interrupt_enable(irq);
}

static inline void interrupt_disable(int irq)
{
	arch_interrupt_disable(irq);
}

static inline void interrupt_enable_sync(void)
{
	arch_interrupt_enable_sync();
}

static inline void interrupt_set(int irq)
{
	arch_interrupt_set(irq);
}

static inline void interrupt_clear(int irq)
{
	arch_interrupt_clear(irq);
}

static inline uint32_t interrupt_get_enabled(void)
{
	return arch_interrupt_get_enabled();
}

static inline uint32_t interrupt_get_status(void)
{
	return arch_interrupt_get_status();
}

static inline void interrupt_global_disable(void)
{
	arch_interrupt_global_disable();
}

static inline void interrupt_global_enable(void)
{
	arch_interrupt_global_enable();
}

#endif

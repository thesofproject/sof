/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#ifndef __ARCH_INTERRUPT_H
#define __ARCH_INTERRUPT_H

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <reef/interrupt-map.h>
#include <stdint.h>
#include <stdlib.h>

static inline int arch_interrupt_register(uint32_t irq,
	void(*handler)(void *arg), void *arg)
{
	irq = REEF_IRQ_NUMBER(irq);
	xthal_set_intclear(0x1 << irq);
	_xtos_set_interrupt_handler_arg(irq, handler, arg);
	return 0;
}

static inline void arch_interrupt_unregister(uint32_t irq)
{
	irq = REEF_IRQ_NUMBER(irq);
	_xtos_set_interrupt_handler_arg(irq, NULL, NULL);
}

/* returns previous mask */
#define arch_interrupt_enable_mask(mask) \
	_xtos_ints_on(mask)

/* returns previous mask */
#define arch_interrupt_disable_mask(mask) \
	_xtos_ints_off(mask)

static inline void arch_interrupt_set(int irq)
{
	irq = REEF_IRQ_NUMBER(irq);
	xthal_set_intset(0x1 << irq);
}

static inline void arch_interrupt_clear(int irq)
{
	irq = REEF_IRQ_NUMBER(irq);
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

	asm volatile("rsil	%0, 5"
		     : "=a" (flags) :: "memory");
	return flags;
}

static inline void arch_interrupt_global_enable(uint32_t flags)
{
	asm volatile("wsr %0, ps; rsync"
		     :: "a" (flags) : "memory");
}

#endif

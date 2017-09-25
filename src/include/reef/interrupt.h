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

static inline uint32_t interrupt_enable(uint32_t irq)
{
	return arch_interrupt_enable_mask(1 << irq);
}

static inline uint32_t interrupt_disable(uint32_t irq)
{
	return arch_interrupt_disable_mask(1 <<irq);
}

static inline void interrupt_set(int irq)
{
	arch_interrupt_set(irq);
}

static inline void interrupt_clear(int irq)
{
	arch_interrupt_clear(irq);
}

static inline uint32_t interrupt_global_disable(void)
{
	return arch_interrupt_global_disable();
}

static inline void interrupt_global_enable(uint32_t flags)
{
	arch_interrupt_global_enable(flags);
}

uint32_t platform_interrupt_get_enabled(void);
void platform_interrupt_clear(uint32_t irq, uint32_t mask);
void platform_interrupt_mask(uint32_t irq, uint32_t mask);
void platform_interrupt_unmask(uint32_t irq, uint32_t mask);

#endif

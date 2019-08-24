/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_INTERRUPT_H
#define __ARCH_INTERRUPT_H

#include <irq.h>

#include <stdint.h>
#include <stdlib.h>

static inline int arch_interrupt_register(int irq,
	void (*handler)(void *arg), void *arg) {return 0; }
static inline void arch_interrupt_unregister(int irq) {}
static inline uint32_t arch_interrupt_enable_mask(uint32_t mask) {return 0; }
static inline uint32_t arch_interrupt_disable_mask(uint32_t mask) {return 0; }

static inline uint32_t arch_interrupt_get_level(void)
{
	uint32_t level;

	__asm__ __volatile__(
		"       rsr.ps %0\n"
		"       extui  %0, %0, 0, 4\n"
		: "=&a" (level) :: "memory");

	return level;
}

static inline void arch_interrupt_set(int irq) {}
static inline void arch_interrupt_clear(int irq) {}
static inline uint32_t arch_interrupt_get_enabled(void) {return 0; }
static inline uint32_t arch_interrupt_get_status(void) {return 0; }
static inline int arch_interrupt_init(void) {return 0; }

static inline uint32_t arch_interrupt_global_disable(void)
{
	return irq_lock();
}

static inline void arch_interrupt_global_enable(uint32_t flags)
{
	irq_unlock(flags);
}

#endif


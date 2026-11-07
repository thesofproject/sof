/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __ARCH_DRIVERS_INTERRUPT_H__
#define __ARCH_DRIVERS_INTERRUPT_H__

#include <stdint.h>

#define PLATFORM_IRQ_HW_NUM   0
#define PLATFORM_IRQ_CHILDREN 0

static inline int arch_interrupt_register(int irq,
	void (*handler)(void *arg), void *arg) {return 0; }
static inline void arch_interrupt_unregister(int irq) {}
static inline uint32_t arch_interrupt_enable_mask(uint32_t mask) {return 0; }
static inline uint32_t arch_interrupt_disable_mask(uint32_t mask) {return 0; }
static inline uint32_t arch_interrupt_get_level(void) { return 0; }
static inline void arch_interrupt_set(int irq) {}
static inline void arch_interrupt_clear(int irq) {}
static inline uint32_t arch_interrupt_get_enabled(void) {return 0; }
static inline uint32_t arch_interrupt_get_status(void) {return 0; }
static inline uint32_t arch_interrupt_global_disable(void) {return 0; }
static inline void arch_interrupt_global_enable(uint32_t flags) {}
static inline int arch_interrupt_init(void) {return 0; }

#endif /* __ARCH_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_INTERRUPT_H__ */

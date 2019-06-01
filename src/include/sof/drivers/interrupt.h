/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __INCLUDE_DRIVERS_INTERRUPT__
#define __INCLUDE_DRIVERS_INTERRUPT__

#include <sof/interrupt.h>

void platform_interrupt_init(void);

struct irq_desc *platform_irq_get_parent(uint32_t irq);
void platform_interrupt_set(int irq);
void platform_interrupt_clear(uint32_t irq, uint32_t mask);
uint32_t platform_interrupt_get_enabled(void);
void platform_interrupt_mask(uint32_t irq, uint32_t mask);
void platform_interrupt_unmask(uint32_t irq, uint32_t mask);

#endif

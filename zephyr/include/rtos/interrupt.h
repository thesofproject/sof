/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * TODO: IMX support, today IRQ is supported via SOF xtos API.
 */

#ifndef __ZEPHYR_RTOS_INTERRUPT_H__
#define __ZEPHYR_RTOS_INTERRUPT_H__

/* TODO: to be removed completely when the following platforms are switched
 * to native drivers.
 */
#if defined(CONFIG_IMX8M) || defined(CONFIG_AMD)
/* imx currently has no IRQ driver in Zephyr so we force to xtos IRQ */
#include "../../../xtos/include/rtos/interrupt.h"
#else

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <stdint.h>

#include <sof/trace/trace.h>

extern struct tr_ctx zephyr_tr;

static inline int interrupt_register(uint32_t irq, void(*handler)(void *arg), void *arg)
{
#ifdef CONFIG_DYNAMIC_INTERRUPTS
	return arch_irq_connect_dynamic(irq, 0, (void (*)(const void *))handler,
					arg, 0);
#else
	LOG_MODULE_DECLARE(zephyr, CONFIG_SOF_LOG_LEVEL);
	tr_err(&zephyr_tr, "Cannot register handler for IRQ %u: dynamic IRQs are disabled",
		irq);
	return -EOPNOTSUPP;
#endif
}

/* unregister an IRQ handler - matches on IRQ number and data ptr */
static inline void interrupt_unregister(uint32_t irq, const void *arg)
{
	/*
	 * There is no "unregister" (or "disconnect") for
	 * interrupts in Zephyr.
	 */
	irq_disable(irq);
}

static inline int interrupt_get_irq(unsigned int irq, const char *cascade)
{
#ifdef CONFIG_IMX8M
	if (cascade == irq_name_level2)
		return SOC_AGGREGATE_IRQ(irq, IRQ_NUM_EXT_LEVEL2);
	if (cascade == irq_name_level5)
		return SOC_AGGREGATE_IRQ(irq, IRQ_NUM_EXT_LEVEL5);

	return SOC_AGGREGATE_IRQ(0, irq);
#else
	return irq;
#endif
}

/* enable an interrupt source - IRQ needs mapped to Zephyr,
 * arg is used to match.
 */
static inline uint32_t interrupt_enable(uint32_t irq, void *arg)
{
	irq_enable(irq);

	return 0;
}

/* disable interrupt */
static inline uint32_t interrupt_disable(uint32_t irq, void *arg)
{
	irq_disable(irq);

	return 0;
}

/* TODO: using SOF versions */
void interrupt_mask(uint32_t irq, unsigned int cpu);
void interrupt_unmask(uint32_t irq, unsigned int cpu);
void interrupt_clear_mask(uint32_t irq, uint32_t mask);

/* handled by Zephyr */
static inline void platform_interrupt_init(void) {}

/* disables all IRQ sources on current core */
#define irq_local_disable(flags) \
	(flags = arch_irq_lock())

/* re-enables IRQ sources on current core */
#define irq_local_enable(flags) \
	arch_irq_unlock(flags)

#endif /* IMX */

#endif /* __ZEPHYR_RTOS_INTERRUPT_H__ */

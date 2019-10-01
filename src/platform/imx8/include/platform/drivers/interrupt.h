/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

#include <sof/bit.h>
#include <config.h>

/* IRQ numbers */
#if CONFIG_INTERRUPT_LEVEL_1

#define IRQ_NUM_SOFTWARE0	8	/* Level 1 */

#define IRQ_MASK_SOFTWARE0	BIT(IRQ_NUM_SOFTWARE0)

#endif

#if CONFIG_INTERRUPT_LEVEL_2

#define IRQ_NUM_TIMER0		2	/* Level 2 */
#define IRQ_NUM_MU		7	/* Level 2 */
#define IRQ_NUM_SOFTWARE1	9	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP0	19	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP1	20	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP2	21	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP3	22	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP4	23	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP5	24	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP6	25	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP7	26	/* Level 2 */

#define IRQ_MASK_TIMER0		BIT(IRQ_NUM_TIMER0)
#define IRQ_MASK_MU		BIT(IRQ_NUM_MU)
#define IRQ_MASK_SOFTWARE1	BIT(IRQ_NUM_SOFTWARE1)
#define IRQ_MASK_IRQSTR_DSP0	BIT(IRQ_NUM_IRQSTR_DSP0)
#define IRQ_MASK_IRQSTR_DSP1	BIT(IRQ_NUM_IRQSTR_DSP1)
#define IRQ_MASK_IRQSTR_DSP2	BIT(IRQ_NUM_IRQSTR_DSP2)
#define IRQ_MASK_IRQSTR_DSP3	BIT(IRQ_NUM_IRQSTR_DSP3)
#define IRQ_MASK_IRQSTR_DSP4	BIT(IRQ_NUM_IRQSTR_DSP4)
#define IRQ_MASK_IRQSTR_DSP5	BIT(IRQ_NUM_IRQSTR_DSP5)
#define IRQ_MASK_IRQSTR_DSP6	BIT(IRQ_NUM_IRQSTR_DSP6)
#define IRQ_MASK_IRQSTR_DSP7	BIT(IRQ_NUM_IRQSTR_DSP7)

#endif

#if CONFIG_INTERRUPT_LEVEL_3

#define IRQ_NUM_TIMER1		3	/* Level 3 */

#define IRQ_MASK_TIMER1		BIT(IRQ_NUM_TIMER1)

#endif

/* 32 HW interrupts + 8 IRQ_STEER lines each with 64 interrupts */
#define PLATFORM_IRQ_HW_NUM	XCHAL_NUM_INTERRUPTS
#define PLATFORM_IRQ_CHILDREN	64

/* irqstr_get_sof_int() - Convert IRQ_STEER interrupt to SOF logical
 * interrupt
 * @irqstr_int Shared IRQ_STEER interrupt
 *
 * Get the SOF interrupt number for a shared IRQ_STEER interrupt number.
 * The IRQ_STEER number is the one specified in the hardware description
 * manuals, while the SOF interrupt number is the one usable with the
 * interrupt_register and interrupt_enable functions.
 *
 * Return: SOF interrupt number
 */
int irqstr_get_sof_int(int irqstr_int);

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

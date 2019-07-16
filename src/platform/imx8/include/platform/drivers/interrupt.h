/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

/* IRQ numbers */
#define IRQ_NUM_TIMER0		2	/* Level 2 */
#define IRQ_NUM_TIMER1		3	/* Level 3 */
#define IRQ_NUM_MU		7	/* Level 2 */
#define IRQ_NUM_SOFTWARE0	8	/* Level 1 */
#define IRQ_NUM_SOFTWARE1	9	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP0	19	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP1	19	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP2	20	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP3	21	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP4	22	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP5	23	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP6	24	/* Level 2 */
#define IRQ_NUM_IRQSTR_DSP7	25	/* Level 2 */

/* IRQ Masks */
#define IRQ_MASK_TIMER0		(1 << IRQ_NUM_TIMER0)
#define IRQ_MASK_TIMER1		(1 << IRQ_NUM_TIMER1)
#define IRQ_MASK_MU		(1 << IRQ_NUM_MU)
#define IRQ_MASK_SOFTWARE0	(1 << IRQ_NUM_SOFTWARE0)
#define IRQ_MASK_SOFTWARE1	(1 << IRQ_NUM_SOFTWARE1)
#define IRQ_MASK_IRQSTR_DSP0	(1 << IRQ_NUM_IRQSTR_DSP0)
#define IRQ_MASK_IRQSTR_DSP1	(1 << IRQ_NUM_IRQSTR_DSP1)
#define IRQ_MASK_IRQSTR_DSP2	(1 << IRQ_NUM_IRQSTR_DSP2)
#define IRQ_MASK_IRQSTR_DSP3	(1 << IRQ_NUM_IRQSTR_DSP3)
#define IRQ_MASK_IRQSTR_DSP4	(1 << IRQ_NUM_IRQSTR_DSP4)
#define IRQ_MASK_IRQSTR_DSP5	(1 << IRQ_NUM_IRQSTR_DSP5)
#define IRQ_MASK_IRQSTR_DSP6	(1 << IRQ_NUM_IRQSTR_DSP6)
#define IRQ_MASK_IRQSTR_DSP7	(1 << IRQ_NUM_IRQSTR_DSP7)

/* no nested interrupts */
#define PLATFORM_IRQ_CHILDREN	0

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

#include <rtos/bit.h>


/* IRQ numbers */
#if CONFIG_XT_INTERRUPT_LEVEL_1

#define IRQ_NUM_SOFTWARE0	8	/* Level 1 */

#define IRQ_MASK_SOFTWARE0	BIT(IRQ_NUM_SOFTWARE0)

#endif

#if CONFIG_XT_INTERRUPT_LEVEL_2

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

#if CONFIG_XT_INTERRUPT_LEVEL_3

#define IRQ_NUM_TIMER1		3	/* Level 3 */

#define IRQ_MASK_TIMER1		BIT(IRQ_NUM_TIMER1)

#endif

/* 32 HW interrupts + 8 IRQ_STEER lines each with 64 interrupts */
#define PLATFORM_IRQ_HW_NUM	XCHAL_NUM_INTERRUPTS
#define PLATFORM_IRQ_CHILDREN	64 /* Each cascaded struct covers 64 IRQs */
/* On the i.MX8MP there are no reserved interrupts in the IRQ_STEER controller,
 * so we have to revert to the usual behavior of moving the IRQ_STEER interrupts
 * after the hardware interrupts.
 *
 * On i.MX8QXP and i.MX8QM this value is 0 so as to hide the reserved initial
 * 32 interrupts by overlapping them with the hardware interrupts.
 *
 * In practice this means that the IRQ_STEER interrupt numbers do not match the
 * SOF internal interrupt numbers exactly; rather IRQ_STEER interrupt 0 will be
 * known as interrupt number 32 within SOF.
 */
#define PLATFORM_IRQ_FIRST_CHILD  0

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

#define IRQSTR_BASE_ADDR	0x30A80000

/* The MASK, SET (unused) and STATUS registers are 160-bit registers
 * split into 5 32-bit registers that we can directly access.
 *
 * The interrupts are mapped in the registers in the following way:
 * Interrupts 0-31 at offset 0
 * Interrupts 32-63 at offset 1
 * Interrupts 64-95 at offset 2
 * Interrupts 96-127 at offset 3
 * Interrupts 128-159 at offset 4
 */

/* This register is only there for HW compatibility; on this platform it
 * does nothing.
 */
#define IRQSTR_CHANCTL			0x00

#define IRQSTR_CH_MASK(n)		(0x04 + 0x04 * (4 - (n)))
#define IRQSTR_CH_SET(n)		(0x18 + 0x04 * (4 - (n)))
#define IRQSTR_CH_STATUS(n)		(0x2C + 0x04 * (4 - (n)))

#define IRQSTR_MASTER_DISABLE		0x40
#define IRQSTR_MASTER_STATUS		0x44

#define IRQSTR_RESERVED_IRQS_NUM	32
#define IRQSTR_IRQS_NUM			192
#define IRQSTR_IRQS_REGISTERS_NUM	5
#define IRQSTR_IRQS_PER_LINE		64

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

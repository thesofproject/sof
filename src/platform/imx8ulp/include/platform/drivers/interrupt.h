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


/* IRQ numbers */
#define IRQ_DSP_SYS_NMI      0
#define IRQ_NUM_SOFTWARE0    1
#define IRQ_NUM_TIMER0       2
#define IRQ_NUM_TIMER1       3
#define DSP_Profiling_IRQ_L3 4
#define DSP_Wer_L3 5
#define DMA2_c0_ErrTranc 6
#define IRQ_NUM_MU2          14
#define IRQ_NUM_MU3          15
#define IRQ_SAI5_NUM         23
#define IRQ_SAI6_NUM         24
#define IRQ_SAI7_NUM         25

#define PLATFORM_IRQ_HW_NUM	XCHAL_NUM_INTERRUPTS
#define PLATFORM_IRQ_CHILDREN	0 /* Each cascaded struct covers 64 IRQs */
/* IMX: Covered steer IRQs are modulo-64 aligned. */
#define PLATFORM_IRQ_FIRST_CHILD  0

// ----------------- need remove 

//#define IRQ_NUM_SOFTWARE0	8	/* Level 1 */
//#define IRQ_NUM_TIMER0		2	/* Level 2 */
//#define IRQ_NUM_MU		7	/* Level 2 */
//#define IRQ_NUM_SOFTWARE1	9	/* Level 2 */
//#define IRQ_NUM_IRQSTR_DSP0	19	/* Level 2 */
//#define IRQ_NUM_IRQSTR_DSP1	20	/* Level 2 */
//#define IRQ_NUM_IRQSTR_DSP2	21	/* Level 2 */
//#define IRQ_NUM_IRQSTR_DSP3	22	/* Level 2 */
//#define IRQ_NUM_IRQSTR_DSP4	23	/* Level 2 */
//#define IRQ_NUM_IRQSTR_DSP5	24	/* Level 2 */
//#define IRQ_NUM_IRQSTR_DSP6	25	/* Level 2 */
//#define IRQ_NUM_IRQSTR_DSP7	26	/* Level 2 */

/*
#define IRQSTR_BASE_ADDR	0x510A0000
#define IRQSTR_CHANCTL			0x00
#define IRQSTR_CH_MASK(n)		(0x04 + 0x04 * (15 - (n)))
#define IRQSTR_CH_SET(n)		(0x44 + 0x04 * (15 - (n)))
#define IRQSTR_CH_STATUS(n)		(0x84 + 0x04 * (15 - (n)))
#define IRQSTR_MASTER_DISABLE		0xC4
#define IRQSTR_MASTER_STATUS		0xC8

#define IRQSTR_RESERVED_IRQS_NUM	32
#define IRQSTR_IRQS_NUM			512
#define IRQSTR_IRQS_REGISTERS_NUM	16
#define IRQSTR_IRQS_PER_LINE		64
*/

/* no irqstr in 8ulp */
#define IRQSTR_CH_MASK(n)		(0x04 + 0x04 * (15 - (n)))
#define IRQSTR_RESERVED_IRQS_NUM	0
#define IRQSTR_IRQS_NUM			0
#define IRQSTR_IRQS_REGISTERS_NUM	0
#define IRQSTR_IRQS_PER_LINE		0

int irqstr_get_sof_int(int irqstr_int);
#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

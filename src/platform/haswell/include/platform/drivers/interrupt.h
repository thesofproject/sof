/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

#define PLATFORM_IRQ_HW_NUM	XCHAL_NUM_INTERRUPTS
#define PLATFORM_IRQ_CHILDREN	0

/* IRQ numbers */
#define IRQ_NUM_EXT_SSP0	0	/* Level 1 */
#define IRQ_NUM_EXT_SSP1	1	/* Level 1 */
#define IRQ_NUM_EXT_OBFF	2	/* Level 1 */
#define IRQ_NUM_EXT_IA		4	/* Level 1 */
#define IRQ_NUM_TIMER1		6	/* Level 1 */
#define IRQ_NUM_SOFTWARE1	7	/* Level 1 */
#define IRQ_NUM_EXT_DMAC0	8	/* Level 2 */
#define IRQ_NUM_EXT_DMAC1	9	/* Level 3 */
#define IRQ_NUM_TIMER2		10	/* Level 3 */
#define IRQ_NUM_SOFTWARE2	11	/* Level 3 */
#define IRQ_NUM_EXT_PARITY	12	/* Level 4 */
#define IRQ_NUM_TIMER3		13	/* Level 5 */
#define IRQ_NUM_NMI		14	/* Level 7 */

/* IRQ Masks */
#define IRQ_MASK_EXT_SSP0	(1 << IRQ_NUM_EXT_SSP0)
#define IRQ_MASK_EXT_SSP1	(1 << IRQ_NUM_EXT_SSP1)
#define IRQ_MASK_EXT_OBFF	(1 << IRQ_NUM_EXT_OBFF)
#define IRQ_MASK_EXT_IA		(1 << IRQ_NUM_EXT_IA)
#define IRQ_MASK_TIMER1		(1 << IRQ_NUM_TIMER1)
#define IRQ_MASK_SOFTWARE1	(1 << IRQ_NUM_SOFTWARE1)
#define IRQ_MASK_EXT_DMAC0	(1 << IRQ_NUM_EXT_DMAC0)
#define IRQ_MASK_EXT_DMAC1	(1 << IRQ_NUM_EXT_DMAC1)
#define IRQ_MASK_TIMER2		(1 << IRQ_NUM_TIMER2)
#define IRQ_MASK_SOFTWARE2	(1 << IRQ_NUM_SOFTWARE2)
#define IRQ_MASK_EXT_PARITY	(1 << IRQ_NUM_EXT_PARITY)
#define IRQ_MASK_TIMER3		(1 << IRQ_NUM_TIMER3)

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

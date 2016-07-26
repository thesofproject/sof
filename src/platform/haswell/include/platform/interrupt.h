/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_PLATFORM_INTERRUPT__
#define __INCLUDE_PLATFORM_INTERRUPT__

#include <stdint.h>

/* IRQ numbers */
#define IRQ_NUM_EXT_SSP0	0
#define IRQ_NUM_EXT_SSP1	1
#define IRQ_NUM_EXT_OBFF	2
#define IRQ_NUM_EXT_IA		4
#define IRQ_NUM_TIMER1		6
#define IRQ_NUM_SOFTWARE1	7
#define IRQ_NUM_EXT_DMAC0	8
#define IRQ_NUM_EXT_DMAC1	9
#define IRQ_NUM_TIMER2		10
#define IRQ_NUM_SOFTWARE2	11
#define IRQ_NUM_EXT_PARITY	12
#define IRQ_NUM_TIMER3		13
#define IRQ_NUM_NMI		14

/* IRQ Masks */
#define IRQ_MASK_EXT_SSP0	(1 << IRQ_NUM_EXT_SSP0)
#define IRQ_MASK_EXT_SSP1	(1 << IRQ_NUM_EXT_SSP1)
#define IRQ_MASK_EXT_OBFF	(1 << IRQ_NUM_EXT_OBFF)
#define IRQ_MASK_EXT_IA		(1 << IRQ_NUM_EXT_IA)
#define IRQ_MASK_TIMER1 	(1 << IRQ_NUM_TIMER1)
#define IRQ_MASK_SOFTWARE1	(1 << IRQ_NUM_SOFTWARE1)
#define IRQ_MASK_EXT_DMAC0 	(1 << IRQ_NUM_EXT_DMAC0)
#define IRQ_MASK_EXT_DMAC1	(1 << IRQ_NUM_EXT_DMAC1)
#define IRQ_MASK_TIMER2		(1 << IRQ_NUM_TIMER2)
#define IRQ_MASK_SOFTWARE2	(1 << IRQ_NUM_SOFTWARE2)
#define IRQ_MASK_EXT_PARITY 	(1 << IRQ_NUM_EXT_PARITY)
#define IRQ_MASK_TIMER2		(1 << IRQ_NUM_TIMER2)

uint32_t platform_interrupt_get_enabled(void);

#endif


/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 AMD.All rights reserved.
 *
 * Author:      Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *              Bala Kishore <balakishore.pati@amd.com>
 */
#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

#include <rtos/bit.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>

#define PLATFORM_IRQ_HW_NUM		9

#define PLATFORM_IRQ_FIRST_CHILD	PLATFORM_IRQ_HW_NUM

#define PLATFORM_IRQ_CHILDREN		0

/* IRQ numbers - wrt Tensilica DSP */
#define IRQ_NUM_SOFTWARE0	1	/* level 1 */

#define IRQ_NUM_TIMER0		0	/* level 1 */

#define IRQ_NUM_EXT_LEVEL3	3	/* level 1 */

#define IRQ_NUM_TIMER1		6	/* level 2 */

#define IRQ_NUM_EXT_LEVEL4	4	/* level 2 */

#define IRQ_NUM_EXT_LEVEL5	5	/* level 3 */

/* IRQ Masks */
#define IRQ_MASK_SOFTWARE0	(1 << IRQ_NUM_SOFTWARE0)

#define IRQ_MASK_TIMER0		(1 << IRQ_NUM_TIMER0)

#define IRQ_MASK_TIMER1		(1 << IRQ_NUM_TIMER1)

#define IRQ_MASK_EXT_LEVEL3	(1 << IRQ_NUM_EXT_LEVEL3)

#define IRQ_MASK_EXT_LEVEL4	(1 << IRQ_NUM_EXT_LEVEL4)

#define IRQ_MASK_EXT_LEVEL5	(1 << IRQ_NUM_EXT_LEVEL5)


#define HOST_TO_DSP_INTR	0x1

#define _XTSTR(x)   # x

#define XTSTR(x)    _XTSTR(x)


/* Enabling flag */
#define INTERRUPT_ENABLE	1

/* Clearing flag */
#define INTERRUPT_CLEAR		0

/* Disable flag */
#define INTERRUPT_DISABLE	0

/* brief Tensilica  Interrupt Levels. */
typedef enum {
	acp_interrupt_level_3 = 0,

	acp_interrupt_level_4,

	acp_interrupt_level_5,

	acp_interrupt_level_nmi,

	acp_interrupt_level_max
} artos_interrupt_levels_t;

/* brief Tensilica  timer control */
typedef enum {
	acp_timer_cntl_disable = 0,

	acp_timer_cntl_oneshot,

	acp_timer_cntl_periodic,

	acp_timer_cntl_max
} artos_timer_control_t;

/* Disable Host to DSP interrupt */
void acp_dsp_sw_intr_disable(void);

void acp_intr_route(void);

void acp_dsp_to_host_intr_trig(void);

void acp_ack_intr_from_host(void);

void acp_dsp_sw_intr_enable(void);

void acp_intr_enable(void);

void acp_intr_disable(void);

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

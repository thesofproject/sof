/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#ifndef __PLATFORM_DRIVERS_TIMER_H__
#define __PLATFORM_DRIVERS_TIMER_H__

#include <rtos/bit.h>
#include <platform/drivers/mt_reg_base.h>


/*-------timer:ostimer0-------*/
enum ostimer {
	OSTIMER0 = 0,
	OSTIMER1,
	OSTIMER2,
	OSTIMER3,
	NR_TMRS
};

#define TIMER_CON(n) (DSP_TIMER_BASE + 0x40 + 0x8 * (n))
#define TIMER_CNT_VAL(n) (DSP_TIMER_BASE + 0x44 + 0x8 * (n))

#define TIMER_ENABLE_BIT	(0x1 << 0)
#define TIMER_IRQ_ENABLE	(0x1 << 1)
#define TIMER_IRQ_STA		(0x1 << 4)
#define TIMER_IRQ_CLEAR		(0x1 << 4)

/**
 * system timer register map
 */
#define CNTCR           (DSP_TIMER_BASE + 0x00)
#define CNTSR           (DSP_TIMER_BASE + 0x04)
#define CNTCV_L         (DSP_TIMER_BASE + 0x08)
#define CNTCV_H         (DSP_TIMER_BASE + 0x0c)
#define CNTWACR         (DSP_TIMER_BASE + 0x10)
#define CNTRACR         (DSP_TIMER_BASE + 0x14)
#define CNTACR_LOCK     (DSP_TIMER_BASE + 0x18)
#define CNTFID0         (DSP_TIMER_BASE + 0x20)
#define CNTFID1         (DSP_TIMER_BASE + 0x24)
#define CNTFID2         (DSP_TIMER_BASE + 0x28)
#define CNTFIDE         (DSP_TIMER_BASE + 0x2c)

#define CNT_EN_BIT BIT(0)
#define CLKSRC_BIT (BIT(8) | BIT(9))
#define CLKSRC_13M_BIT BIT(8)
#define COMP_BIT (BIT(10) | BIT(11) | BIT(12))
#define COMP_20_25_EN_BIT (BIT(11) | BIT(12))

#define CNT_EN          (0x1 << 0)
#define CNTTVAL_EN      (0x1 << 0)
#define CNTIRQ_EN       (0x1 << 1)
#define CNTIRQ_STACLR   (0x1 << 4)
#define CNTMODE_REPEAT  (0x1 << 8)


#define DELAY_TIMER_1US_TICK       (13U) // (13MHz)
#define DELAY_TIMER_1MS_TICK       (13000U) // (13MHz)
#define TIME_TO_TICK_US(us) ((us)*DELAY_TIMER_1US_TICK)
#define TIME_TO_TICK_MS(ms) ((ms)*DELAY_TIMER_1MS_TICK)

#define SYSTICK_TIMER_IRQ           L1_DSP_TIMER_IRQ0_B


#endif /* __PLATFORM_DRIVERS_TIMER_H__ */

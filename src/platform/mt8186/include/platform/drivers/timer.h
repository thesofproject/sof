// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek. All rights reserved.
//
// Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>
//         Tinghan Shen <tinghan.shen@mediatek.com>

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

#define MTK_TIMER_CON(n)		(MTK_DSP_OS_TIMER_BASE + 0x0 + 0x10 * (n))
#define MTK_TIMER_RST_VAL(n)		(MTK_DSP_OS_TIMER_BASE + 0x4 + 0x10 * (n))
#define MTK_TIMER_CUR_VAL(n)		(MTK_DSP_OS_TIMER_BASE + 0x8 + 0x10 * (n))
#define MTK_TIMER_IRQ_ACK(n)		(MTK_DSP_OS_TIMER_BASE + 0xC + 0x10 * (n))

#define MTK_TIMER_ENABLE_BIT		BIT(0)
#define MTK_TIMER_IRQ_ENABLE		BIT(0)
#define MTK_TIMER_IRQ_STA		BIT(4)
#define MTK_TIMER_IRQ_CLEAR		BIT(5)
#define MTK_TIMER_CLKSRC_BIT		(BIT(4) | BIT(5))

#define MTK_TIMER_CLK_SRC_OFFSET	4
#define MTK_TIMER_CLK_SRC_CLK_26M	0
#define MTK_TIMER_CLK_SRC_BCLK		BIT(4)
#define MTK_TIMER_CLK_SRC_PCLK		BIT(5)

/*-------platform_timer: 64 bit systimer-------*/
#define MTK_OSTIMER_CON			(MTK_DSP_OS_TIMER_BASE + 0x80)
#define MTK_OSTIMER_INIT_L		(MTK_DSP_OS_TIMER_BASE + 0x84)
#define MTK_OSTIMER_INIT_H		(MTK_DSP_OS_TIMER_BASE + 0x88)
#define MTK_OSTIMER_CUR_L		(MTK_DSP_OS_TIMER_BASE + 0x8C)
#define MTK_OSTIMER_CUR_H		(MTK_DSP_OS_TIMER_BASE + 0x90)
#define MTK_OSTIMER_TVAL		(MTK_DSP_OS_TIMER_BASE + 0x94)
#define MTK_OSTIMER_IRQ_ACK		(MTK_DSP_OS_TIMER_BASE + 0x98)

#define MTK_OSTIMER_EN_BIT			BIT(0)

#endif /* __PLATFORM_DRIVERS_TIMER_H__ */

/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Mediatek
 *
 * Author: Author: Fengquan Chen <fengquan.chen@mediatek.com>
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

#define TIMER_CON(n) (MTK_DSP_OSTIMER_BASE + 0x0 + 0x10 * (n))
#define TIMER_RST_VAL(n) (MTK_DSP_OSTIMER_BASE + 0x4 + 0x10 * (n))
#define TIMER_CUR_VAL(n) (MTK_DSP_OSTIMER_BASE + 0x8 + 0x10 * (n))
#define TIMER_IRQ_ACK(n) (MTK_DSP_OSTIMER_BASE + 0xC + 0x10 * (n))

#define TIMER_ENABLE_BIT (0x1 << 0)
#define TIMER_IRQ_ENABLE (0x1 << 0)
#define TIMER_IRQ_STA (0x1 << 4)
#define TIMER_IRQ_CLEAR (0x1 << 5)
#define TIMER_CLKSRC_BIT (0x3 << 4)

#define TIMER_CLK_SRC_CLK_32K 0x00
#define TIMER_CLK_SRC_CLK_26M 0x01
#define TIMER_CLK_SRC_BCLK 0x02
#define TIMER_CLK_SRC_PCLK 0x03

#define TIMER_CLK_SRC_SHIFT 4

#define OSTIMER_CON (MTK_DSP_OSTIMER_BASE + 0x80)
#define OSTIMER_INIT_L (MTK_DSP_OSTIMER_BASE + 0x84)
#define OSTIMER_INIT_H (MTK_DSP_OSTIMER_BASE + 0x88)
#define OSTIMER_CUR_L (MTK_DSP_OSTIMER_BASE + 0x8C)
#define OSTIMER_CUR_H (MTK_DSP_OSTIMER_BASE + 0x90)
#define OSTIMER_TVAL (MTK_DSP_OSTIMER_BASE + 0x94)
#define OSTIMER_IRQ_ACK (MTK_DSP_OSTIMER_BASE + 0x98)

/*-------platform_timer: 64 bit systimer-------*/
#define CNTCR (MTK_DSP_TIMER_BASE + 0x00)
#define CNTSR (MTK_DSP_TIMER_BASE + 0x04)
#define CNTCV_L (MTK_DSP_TIMER_BASE + 0x08)
#define CNTCV_H (MTK_DSP_TIMER_BASE + 0x0c)
#define CNTWACR (MTK_DSP_TIMER_BASE + 0x10)
#define CNTRACR (MTK_DSP_TIMER_BASE + 0x14)

#define CNT_EN_BIT BIT(0)
#define CLKSRC_BIT (BIT(8) | BIT(9))
#define CLKSRC_13M_BIT BIT(8)
#define COMP_BIT (BIT(10) | BIT(11) | BIT(12))
#define COMP_20_25_EN_BIT (BIT(11) | BIT(12))

#endif /* __PLATFORM_DRIVERS_TIMER_H__ */

/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

#include <stdint.h>
#include <platform/drivers/mt_reg_base.h>

#define PLATFORM_IRQ_HW_NUM		XCHAL_NUM_INTERRUPTS
#define PLATFORM_IRQ_FIRST_CHILD	PLATFORM_IRQ_HW_NUM
#define PLATFORM_IRQ_CHILDREN		(LX_MCU_IRQ_B - LX_CQDMA_IRQ0_B + 1)

#define IRQ_EXT_MASK			0x3F0

/* interrupt table */
#define MTK_DSP_EXT_IRQ0		0
#define L1_INT_IRQ_B			1 // (INT_LEVEL(1) | IRQ_INVALID)
#define L1_DSP_TIMER_IRQ0_B		2 // (INT_LEVEL(2) | INTERRUPT_ID(0))
#define L1_DSP_TIMER_IRQ1_B		3 // (INT_LEVEL(3) | INTERRUPT_ID(1))
#define L1_DSP_TIMER_IRQ2_B		4 // (INT_LEVEL(4) | INTERRUPT_ID(2))
#define L1_DSP_TIMER_IRQ3_B		5 // (INT_LEVEL(5) | INTERRUPT_ID(3))
#define MTK_DSP_EXT_IRQ6		6
#define MTK_DSP_EXT_IRQ7		7
#define MTK_DSP_EXT_IRQ8		8
#define MTK_DSP_EXT_IRQ9		9
#define MTK_DSP_EXT_IRQ10		10
#define MTK_DSP_EXT_IRQ11		11
#define MTK_DSP_EXT_IRQ12		12
#define MTK_DSP_EXT_IRQ13		13
#define MTK_DSP_EXT_IRQ14		14
#define MTK_DSP_EXT_IRQ15		15
/* HiFi internal interrupts */
#define MTK_DSP_IRQ_TIMER0		16
#define MTK_DSP_IRQ_TIMER1		17
#define MTK_DSP_IRQ_TIMER2		18
#define MTK_DSP_IRQ_WRITE_ERROR		19
#define MTK_DSP_IRQ_PROFILING		20
#define MTK_DSP_IRQ_SOFTWARE0		21
#define MTK_DSP_IRQ_SOFTWARE1		22
/* HiFi external interrupts */
#define MTK_DSP_EXT_INT23		23
#define MTK_DSP_EXT_INT24		24

/* group 1 cascaded IRQ trigger L1_INT_IRQ_B */
#define IRQ_EXT_GROUP1_BASE		L1_INT_IRQ_B
#define LX_CQDMA_IRQ0_B			25 // (INT_LEVEL(1) | INTERRUPT_ID(4))
#define LX_CQDMA_IRQ1_B			26 // (INT_LEVEL(1) | INTERRUPT_ID(5))
#define LX_CQDMA_IRQ2_B			27 // (INT_LEVEL(1) | INTERRUPT_ID(6))
#define LX_UART_IRQ_B			28 // (INT_LEVEL(1) | INTERRUPT_ID(7))
#define LX_AFE_IRQ_B			29 // (INT_LEVEL(1) | INTERRUPT_ID(8))
#define LX_MCU_IRQ_B			30 // (INT_LEVEL(1) | INTERRUPT_ID(9))

int mtk_irq_group_id(uint32_t irq);

/* L1_DSP_TIMER_IRQ0_B corresponds to bit# 0 of RG_DSP_IRQ_EN */
#define IRQ_EXT_BIT_OFFSET		(-2)

/* base_irq = 25; (LX_CQDMA_IRQ0_B - base_irq) corresponds to bit# 4 of RG_DSP_IRQ_EN */
#define IRQ_EXT_GROUP1_BIT_OFFSET	(4)

#define	RG_DSP_IRQ_EN			DSP_RG_INT_EN_CTL0
#define	RG_DSP_IRQ_STATUS		DSP_RG_INT_STATUS0

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

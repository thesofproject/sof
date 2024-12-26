/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

#include <stdint.h>
#include <platform/drivers/mt_reg_base.h>
#include <platform/drivers/intc.h>

#define PLATFORM_IRQ_HW_NUM		XCHAL_NUM_INTERRUPTS
#define PLATFORM_IRQ_FIRST_CHILD	PLATFORM_IRQ_HW_NUM
#define PLATFORM_IRQ_CHILDREN		32

/* MTK_ADSP_IRQ_MASK */
#define MTK_DSP_OUT_IRQ_MASK		0x3FF

#define HIFI_IRQ_MAX_CHANNEL   26

#define DEFALUT_WAKEUP_SRC_MASK_BIT    0x001A5E13
#define DEFAULT_WAKEUP_SRC_EN0_BIT     0xA046B550
#define DEFAULT_WAKEUP_SRC_EN1_BIT     0x00002148

#define MTK_DSP_IRQ_MBOX_C0         2
#define MTK_DSP_IRQ_OSTIMER32_C0	1
#define IPC0_IRQn         IPC_C0_IRQn
#define C2C_SW_IRQn       C2C_SW_C0_IRQn
#define CCIF3_IRQn        CCIF3_C0_IRQn
#define DMA_IRQn          DMA_C0_IRQn
#define AUDIO_IRQn        AUDIO_C0_IRQn
#define HIFI5_WDT_IRQn    HIFI5_WDT_C0_IRQn
#define APU_MBOX_IRQn     APU_MBOX_C0_IRQn
#define PWR_ON_CORE_IRQn  PWR_ON_C0_IRQ
#define WAKEUP_SRC_IRQn   WAKEUP_SRC_C0_IRQn
#define AXI_DMA_CH0_IRQn  AXI_DMA0_IRQn
#define AXI_DMA_CH1_IRQn  AXI_DMA2_IRQn

/* following interrupts are HiFi internal interrupts */
#define MTK_DSP_IRQ_NMI                 25
#define MTK_DSP_IRQ_PROFILING           29
#define MTK_DSP_IRQ_WERR                30
#define MTK_DSP_IRQ_SW                  31
#define MTK_MAX_IRQ_NUM			59

/* grouped mailbox IRQ */
#define MTK_DSP_IRQ_MBOX0		59
#define MTK_DSP_IRQ_MBOX1		60
#define MTK_DSP_IRQ_MBOX2		61
#define MTK_DSP_IRQ_MBOX3		62
#define MTK_DSP_IRQ_MBOX4		63

#define	MTK_DSP_MBOX_MASK		0xF

int mtk_irq_group_id(uint32_t irq);
void intc_irq_unmask(enum IRQn_Type irq);
void intc_irq_mask(enum IRQn_Type irq);
int intc_irq_enable(enum IRQn_Type irq);
int intc_irq_disable(enum IRQn_Type irq);

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */

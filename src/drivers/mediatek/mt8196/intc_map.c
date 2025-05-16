// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */

#include <sof/audio/component_ext.h>
#include <rtos/interrupt.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <platform/drivers/timer.h>
#include <platform/drivers/interrupt.h>
#include <ipc/stream.h>
#include <errno.h>
#include <stdint.h>

const unsigned char grp_pri[INTC_GRP_NUM] = {
	INTC_GRP0_LEVEL, INTC_GRP1_LEVEL, INTC_GRP2_LEVEL, INTC_GRP3_LEVEL,
	INTC_GRP4_LEVEL, INTC_GRP5_LEVEL, INTC_GRP6_LEVEL, INTC_GRP7_LEVEL,
	INTC_GRP8_LEVEL, INTC_GRP9_LEVEL, INTC_GRP10_LEVEL, INTC_GRP11_LEVEL,
	INTC_GRP12_LEVEL, INTC_GRP13_LEVEL, INTC_GRP14_LEVEL, INTC_GRP15_LEVEL,
};

const uint8_t irq2grp_map[IRQ_MAX_CHANNEL] = {
	[CCU_IRQn]           = INTC_GRP1,
	[SCP_IRQn]           = INTC_GRP1,
	[SPM_IRQn]           = INTC_GRP1,
	[PCIE_IRQn]          = INTC_GRP1,
	[INFRA_HANG_IRQn]    = INTC_GRP1,
	[PERI_TIMEOUT_IRQn]  = INTC_GRP1,
	[MBOX_C0_IRQn]       = INTC_GRP2,
	[MBOX_C1_IRQn]       = INTC_GRP2,
	[TIMER0_IRQn]        = INTC_GRP1,
	[TIMER1_IRQn]        = INTC_GRP1,
	[IPC_C0_IRQn]        = INTC_GRP1,
	[IPC_C1_IRQn]        = INTC_GRP1,
	[IPC1_RSV_IRQn]      = INTC_GRP1,
	[C2C_SW_C0_IRQn]     = INTC_GRP1,
	[C2C_SW_C1_IRQn]     = INTC_GRP1,
	[UART_IRQn]          = INTC_GRP12,
	[UART_BT_IRQn]       = INTC_GRP12,
	[LATENCY_MON_IRQn]   = INTC_GRP11,
	[BUS_TRACKER_IRQn]   = INTC_GRP13,
	[USB0_IRQn]          = INTC_GRP8,
	[USB1_IRQn]          = INTC_GRP8,
	[SCPVOW_IRQn]        = NO_GRP,
	[CCIF3_C0_IRQn]      = INTC_GRP8,
	[CCIF3_C1_IRQn]      = INTC_GRP8,
	[PWR_CTRL_IRQn]      = NO_GRP,
	[DMA_C0_IRQn]        = INTC_GRP10,
	[DMA_C1_IRQn]        = NO_GRP,
	[AXI_DMA0_IRQn]      = INTC_GRP9,
	[AXI_DMA1_IRQn]      = NO_GRP,
	[AUDIO_C0_IRQn]      = INTC_GRP10,
	[AUDIO_C1_IRQn]      = INTC_GRP10,
	[HIFI5_WDT_C0_IRQn]  = INTC_GRP13,
	[HIFI5_WDT_C1_IRQn]  = INTC_GRP13,
	[APU_MBOX_C0_IRQn]   = INTC_GRP0,
	[APU_MBOX_C1_IRQn]   = INTC_GRP0,
	[TIMER2_IRQn]        = INTC_GRP13,
	[PWR_ON_C0_IRQ]      = INTC_GRP13,
	[PWR_ON_C1_IRQ]      = INTC_GRP13,
	[WAKEUP_SRC_C0_IRQn] = INTC_GRP13,
	[WAKEUP_SRC_C1_IRQn] = INTC_GRP13,
	[WDT_IRQn]           = NO_GRP,
	[CONNSYS1_IRQn]      = INTC_GRP3,
	[CONNSYS3_IRQn]      = INTC_GRP3,
	[CONNSYS4_IRQn]      = INTC_GRP3,
	[CONNSYS2_IRQn]      = INTC_GRP3,
	[IPIC_IRQn]          = INTC_GRP1,
	[AXI_DMA2_IRQn]      = INTC_GRP9,
	[AXI_DMA3_IRQn]      = NO_GRP,
	[APSRC_DDREN_IRQn]   = INTC_GRP4,
	[LAT_MON_EMI_IRQn]   = INTC_GRP11,
	[LAT_MON_INFRA_IRQn] = INTC_GRP11,
	[DEVAPC_VIO_IRQn]    = INTC_GRP11,
	[AO_INFRA_HANG_IRQn] = NO_GRP,
	[BUS_TRA_EMI_IRQn]   = INTC_GRP13,
	[BUS_TRA_INFRA_IRQn] = INTC_GRP13,
	[L2SRAM_VIO_IRQn]    = INTC_GRP11,
	[L2SRAM_SETERR_IRQn] = INTC_GRP11,
	[PCIERC_GRP2_IRQn]   = INTC_GRP8,
	[PCIERC_GRP3_IRQn]   = INTC_GRP8,
};

const uint8_t grp2hifi_irq_map[INTC_GRP_NUM] = {
	[INTC_GRP0]  = 0,
	[INTC_GRP1]  = 1,
	[INTC_GRP2]  = 2,
	[INTC_GRP3]  = 3,
	[INTC_GRP4]  = 4,
	[INTC_GRP5]  = 5,
	[INTC_GRP6]  = 7,
	[INTC_GRP7]  = 8,
	[INTC_GRP8]  = 9,
	[INTC_GRP9]  = 10,
	[INTC_GRP10] = 11,
	[INTC_GRP11] = 16,
	[INTC_GRP12] = 17,
	[INTC_GRP13] = 18,
	[INTC_GRP14] = 20,
	[INTC_GRP15] = 21,
};

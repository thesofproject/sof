/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */
#ifndef MTK_INTC_H
#define MTK_INTC_H

#include <rtos/bit.h>
#include <stdint.h>
#include <platform/drivers/mt_reg_base.h>

enum IRQn_Type {
	CCU_IRQn           = 0,
	SCP_IRQn           = 1,
	SPM_IRQn           = 2,
	PCIE_IRQn          = 3,
	INFRA_HANG_IRQn    = 4,
	PERI_TIMEOUT_IRQn  = 5,
	MBOX_C0_IRQn       = 6,
	MBOX_C1_IRQn       = 7,
	TIMER0_IRQn        = 8,
	TIMER1_IRQn        = 9,
	IPC_C0_IRQn        = 10,
	IPC_C1_IRQn        = 11,
	IPC1_RSV_IRQn      = 12,
	C2C_SW_C0_IRQn     = 13,
	C2C_SW_C1_IRQn     = 14,
	UART_IRQn          = 15,
	UART_BT_IRQn       = 16,
	LATENCY_MON_IRQn   = 17,
	BUS_TRACKER_IRQn   = 18,
	USB0_IRQn          = 19,
	USB1_IRQn          = 20,
	SCPVOW_IRQn        = 21,
	CCIF3_C0_IRQn      = 22,
	CCIF3_C1_IRQn      = 23,
	PWR_CTRL_IRQn      = 24,
	DMA_C0_IRQn        = 25,
	DMA_C1_IRQn        = 26, // no use as gdma only has one set
	AXI_DMA0_IRQn      = 27,
	AXI_DMA1_IRQn      = 28,
	AUDIO_C0_IRQn      = 29,
	AUDIO_C1_IRQn      = 30,
	HIFI5_WDT_C0_IRQn  = 31,
	HIFI5_WDT_C1_IRQn  = 32,
	APU_MBOX_C0_IRQn   = 33,
	APU_MBOX_C1_IRQn   = 34,
	TIMER2_IRQn        = 35,
	PWR_ON_C0_IRQ      = 36,
	PWR_ON_C1_IRQ      = 37,
	WAKEUP_SRC_C0_IRQn = 38,
	WAKEUP_SRC_C1_IRQn = 39,
	WDT_IRQn           = 40,
	CONNSYS1_IRQn      = 41, // BTCVSD
	CONNSYS3_IRQn      = 42, // BLEISO
	CONNSYS4_IRQn      = 43, // ISOCH, bt2dsp_isoch_irq_mask
	CONNSYS2_IRQn      = 44, // A2DP
	IPIC_IRQn          = 45,
	AXI_DMA2_IRQn      = 46,
	AXI_DMA3_IRQn      = 47,
	APSRC_DDREN_IRQn   = 48,
	LAT_MON_EMI_IRQn   = 49,
	LAT_MON_INFRA_IRQn = 50,
	DEVAPC_VIO_IRQn    = 51,
	AO_INFRA_HANG_IRQn = 52,
	BUS_TRA_EMI_IRQn   = 53,
	BUS_TRA_INFRA_IRQn = 54,
	L2SRAM_VIO_IRQn    = 55,
	L2SRAM_SETERR_IRQn = 56,
	PCIERC_GRP2_IRQn   = 57,
	PCIERC_GRP3_IRQn   = 58,
	IRQ_MAX_CHANNEL    = 59,
	NO_IRQ             = 0xFFFFFFFFU, // -1
};

#define INTC_GRP_LEN                2
#define INTC_GRP_GAP                3 // size of group = 2 words = 8 bytes
#define WORD_LEN                    32
#define INTC_WORD(irq)              ((irq) >> 5)
#define INTC_BIT(irq)               (1 << ((irq) & 0x1F))
#define INTC_WORD_OFS(word)         ((word) << 2)
#ifndef INTC_GROUP_OFS
#define INTC_GROUP_OFS(grp)         ((grp) << INTC_GRP_GAP)
#endif

// #define INTC_IRQ_RAW_STA(word)      (INTC_IRQ_RAW_STA0 + INTC_WORD_OFS(word))
#define INTC_IRQ_STA(word)          (INTC_IRQ_STA0 + INTC_WORD_OFS(word))
#define INTC_IRQ_EN(word)           (INTC_IRQ_EN0 + INTC_WORD_OFS(word))
#define INTC_IRQ_WAKE_EN(word)      (INTC_IRQ_WAKE_EN0 + INTC_WORD_OFS(word))
#define INTC_IRQ_STAGE1_EN(word)    (INTC_IRQ_STAGE1_EN0 + INTC_WORD_OFS(word))
#define INTC_IRQ_POL(word)          (INTC_IRQ_POL0 + INTC_WORD_OFS(word))
#define INTC_IRQ_GRP(grp, word)     (INTC_IRQ_GRP0_0 + INTC_GROUP_OFS(grp)\
				     + INTC_WORD_OFS(word))
#define INTC_IRQ_GRP_STA(grp, word) (INTC_IRQ_GRP0_STA0 + INTC_GROUP_OFS(grp)\
				     + INTC_WORD_OFS(word))

/* intc group level */
#define INTC_GRP0_LEVEL             XCHAL_INT0_LEVEL
#define INTC_GRP1_LEVEL             XCHAL_INT1_LEVEL
#define INTC_GRP2_LEVEL             XCHAL_INT2_LEVEL
#define INTC_GRP3_LEVEL             XCHAL_INT3_LEVEL
#define INTC_GRP4_LEVEL             XCHAL_INT4_LEVEL
#define INTC_GRP5_LEVEL             XCHAL_INT5_LEVEL
#define INTC_GRP6_LEVEL             XCHAL_INT7_LEVEL
#define INTC_GRP7_LEVEL             XCHAL_INT8_LEVEL
#define INTC_GRP8_LEVEL             XCHAL_INT9_LEVEL
#define INTC_GRP9_LEVEL             XCHAL_INT10_LEVEL
#define INTC_GRP10_LEVEL            XCHAL_INT11_LEVEL
#define INTC_GRP11_LEVEL            XCHAL_INT16_LEVEL
#define INTC_GRP12_LEVEL            XCHAL_INT17_LEVEL
#define INTC_GRP13_LEVEL            XCHAL_INT18_LEVEL
#define INTC_GRP14_LEVEL            XCHAL_INT20_LEVEL
#define INTC_GRP15_LEVEL            XCHAL_INT21_LEVEL


enum INTC_GROUP {
	INTC_GRP0 = 0,
	INTC_GRP1,
	INTC_GRP2,
	INTC_GRP3,
	INTC_GRP4,
	INTC_GRP5,
	INTC_GRP6,
	INTC_GRP7,
	INTC_GRP8,
	INTC_GRP9,
	INTC_GRP10,
	INTC_GRP11,
	INTC_GRP12,
	INTC_GRP13,
	INTC_GRP14,
	INTC_GRP15,
	INTC_GRP_NUM,
	NO_GRP,
};

enum INTC_POL {
	INTC_POL_HIGH = 0x0,
	INTC_POL_LOW = 0x1,
	INTC_POL_NUM,
};

struct intc_irq_desc_t {
	uint8_t id;
	uint8_t group;
	uint8_t pol;
};

struct intc_desc_t {
	uint32_t int_en[INTC_GRP_LEN];
	uint32_t grp_irqs[INTC_GRP_NUM][INTC_GRP_LEN];
	struct intc_irq_desc_t irqs[IRQ_MAX_CHANNEL];
};

struct intc_irq_config {
	uint32_t int_en[INTC_GRP_LEN];
};

struct intc_grp_config {
	uint32_t grp_irq[INTC_GRP_NUM][INTC_GRP_LEN];
};

struct intc_coreoff_wake_en_config {
	uint32_t wake_en[INTC_GRP_LEN];
};

struct intc_sleep_wake_en_config {
	uint32_t wake_en[INTC_GRP_LEN];
};

extern const unsigned char grp_pri[INTC_GRP_NUM];
extern const uint8_t irq2grp_map[IRQ_MAX_CHANNEL];
extern const uint8_t grp2hifi_irq_map[INTC_GRP_NUM];
void intc_init(void);

#ifdef CFG_TICKLESS_SUPPORT
extern struct intc_sleep_wake_en_config sleep_wakeup_src_en;
#endif
#ifdef CFG_CORE_OFF_SUPPORT
extern struct intc_coreoff_wake_en_config coreoff_wakeup_src_en;
#endif

#endif /* INTC_H */


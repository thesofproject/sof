/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 *         Xiuli Pan <xiuli.pan@linux.intel.com>
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#ifdef CONFIG_SOC_SERIES_INTEL_ACE1X
#include <ace_v1x-regs.h>
#endif /* CONFIG_SOC_SERIES_INTEL_ACE1X */

#define PLATFORM_RESET_MHE_AT_BOOT	1

#define PLATFORM_MEM_INIT_AT_BOOT	1

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <sof/drivers/interrupt.h>
#include <sof/lib/clk.h>
#include <sof/lib/mailbox.h>
#include <stddef.h>
#include <stdint.h>

struct ll_schedule_domain;
struct timer;

/*! \def PLATFORM_DEFAULT_CLOCK
 *  \brief clock source for audio pipeline
 *
 *  There are two types of clock: cpu clock which is a internal clock in
 *  xtensa core, and ssp clock which is provided by external HW IP.
 *  The choice depends on HW features on different platform
 */
#define PLATFORM_DEFAULT_CLOCK CLK_SSP

#define MAX_GPDMA_COUNT 2

/* Host page size */
#define HOST_PAGE_SIZE		4096
#define PLATFORM_PAGE_TABLE_SIZE	256

/* ACE interrupt  */
#define IRQ_LVL_MASK 0x3F
#define IRQ_GET_ID(x, lvl) ((x+1) << (6*(lvl-1))) //max 63 IRQs on each level
#define IS_IRQ_LVL(x, lvl) ((x) & (IRQ_LVL_MASK << (6*(lvl-1))))
#define IRQ_VIRTUAL_TO_PHYSICAL(x, lvl) ((x >> (6*(lvl-1)))-1)
#define LVL1 1
#define LVL2 2
#define LVL3 3
#define LVL4 4
#define LVL5 5

/* IDC Interrupt */
#ifdef CONFIG_SOC_SERIES_INTEL_ACE1X
#define PLATFORM_IDC_INTERRUPT		MTL_IRQ_TO_ZEPHYR(MTL_INTL_IDCA)
#else
#define PLATFORM_IDC_INTERRUPT		IRQ_EXT_IDC_LVL2
#endif /* CONFIG_SOC_SERIES_INTEL_ACE1X */
#define PLATFORM_IDC_INTERRUPT_NAME	irq_name_level2

/* IPC Interrupt */
#ifdef CONFIG_SOC_SERIES_INTEL_ACE1X
#define PLATFORM_IPC_INTERRUPT		MTL_IRQ_TO_ZEPHYR(MTL_INTL_HIPC)
#else
#define PLATFORM_IPC_INTERRUPT		IRQ_GET_ID(IRQ_EXT_IPC_LVL2, LVL2)
#endif /* CONFIG_SOC_SERIES_INTEL_ACE1X */
#define PLATFORM_IPC_INTERRUPT_NAME	irq_name_level2
/* Platform timer Interrupt */
#define PLATFORM_TIMER_IRQ                10
#define PLATFORM_TIMER_INTERRUPT          IRQ_GET_ID(PLATFORM_TIMER_IRQ, LVL2)

/* pipeline IRQ */
#define PLATFORM_SCHEDULE_IRQ		IRQ_NUM_SOFTWARE2
#define PLATFORM_SCHEDULE_IRQ_NAME	NULL

/* Platform stream capabilities */
#define PLATFORM_MAX_CHANNELS	8
#define PLATFORM_MAX_STREAMS	16

/* local buffer size of DMA tracing */
#define DMA_TRACE_LOCAL_SIZE	(HOST_PAGE_SIZE * 2)

/* trace bytes flushed during panic */
#define DMA_FLUSH_TRACE_SIZE    (MAILBOX_TRACE_SIZE >> 2)

/* the interval of DMA trace copying */
#define DMA_TRACE_PERIOD		500000

/*
 * the interval of reschedule DMA trace copying in special case like half
 * fullness of local DMA trace buffer
 */
#define DMA_TRACE_RESCHEDULE_TIME	500

/* DSP default delay in cycles */
#define PLATFORM_DEFAULT_DELAY	12

/* minimal L1 exit time in cycles */
#define PLATFORM_FORCE_L1_EXIT_TIME	985

/* the SSP port fifo depth */
#define SSP_FIFO_DEPTH		16

/* the watermark for the SSP fifo depth setting */
#define SSP_FIFO_WATERMARK	8

/* minimal SSP port delay in cycles */
#define PLATFORM_SSP_DELAY	1600

/* timeout tries and delay for powering up secondary core */
#define PLATFORM_PM_RUNTIME_DSP_TRIES 32
#define PLATFORM_PM_RUNTIME_DSP_DELAY 256

/* 0x12BF magic number id from:
 * DMIC_HW_IOCLK / CONFIG_DMIC_SYNC_PERIOD_VALUE.
 * From spec: E.g. for 19.2 MHz XTAL oscillator clock, 4 KHz sync period,
 * the value to be programmed is 12BFh.
 */
#define PLATFORM_DMIC_SYNC_PERIOD	0x12BF

union DxINTIPPTR {
		uint32_t full;
		struct {
				/**
				 * IP Pointer
				 * type: RO, rst: 07 AC00h, rst domain: nan
				 *
				 * This field contains the offset to the IP.
				 */
				uint32_t      ptr : 21;
				/**
				 * IP Version
				 * type: RO, rst: 000b, rst domain: nan
				 *
				 * This field indicates the version of the IP.
				 */
				uint32_t      ver :  3;
				/**
				 * Reserved
				 * type: RO, rst: 00h, rst domain: nan
				 */
				uint32_t   rsvd31 :  8;
		} part;
};

struct DW_ICTL_REGS {
		// 0x0
		uint32_t inten_l;
		uint32_t inten_h;
		uint32_t intmask_l;
		uint32_t intmask_h;

		// 0x10
		uint32_t intforce_l;
		uint32_t intforce_h;
		uint32_t rawstatus_l;
		uint32_t rawstatus_h;

		// 0x20
		uint32_t status_l;
		uint32_t status_h;
		uint32_t maskstatus_l;
		uint32_t maskstatus_h;

		// 0x30
		uint32_t finalstatus_l;
		uint32_t finalstatus_h;

		// 0x38 (IRQ_VECTOR_* regs - not implemented)
		uint32_t _rsvd0[(0xc0 - 0x38) / sizeof(uint32_t)];

		// 0xc0
		uint32_t fiq_inten;
		uint32_t fiq_intmask;
		uint32_t fiq_intforce;
		uint32_t fiq_rawstatus;

		// 0xd0
		uint32_t fiq_status;
		uint32_t fiq_finalstatus;
		uint32_t plevel;
		uint32_t _rsvd1;

		// 0xe0
		uint32_t ictl_version_id;
};

/* Platform defined trace code */
static inline void platform_panic(uint32_t p)
{
	mailbox_sw_reg_write(SRAM_REG_FW_STATUS, p & 0x3fffffff);
	ipc_write(IPC_DIPCIDD, MAILBOX_EXCEPTION_OFFSET + 2 * 0x20000);
	ipc_write(IPC_DIPCIDR, 0x80000000 | (p & 0x3fffffff));
}

/**
 * \brief Platform specific CPU entering idle.
 * May be power-optimized using platform specific capabilities.
 * @param level Interrupt level.
 */
void platform_wait_for_interrupt(int level);

extern intptr_t _module_init_start;
extern intptr_t _module_init_end;

/*
 * MTL ACE memory register defines
 * TODO: consider moving below defines to some other place
 */
#define CONFIG_ADSP_L2HSBxPM_ADDRESS(x)  (0x17A800 + 0x0008 * (x))
#define CONFIG_ADSP_HSxPGCTL_ADDRESS(x)  (CONFIG_ADSP_L2HSBxPM_ADDRESS(x) + 0x0000)
#define CONFIG_ADSP_HSxRMCTL_ADDRESS(x)  (CONFIG_ADSP_L2HSBxPM_ADDRESS(x) + 0x0001)
#define CONFIG_ADSP_HSxPGISTS_ADDRESS(x) (CONFIG_ADSP_L2HSBxPM_ADDRESS(x) + 0x0004)

union HSxPGCTL {
	uint8_t full;
	struct {
		/**
		 * L2 Local Memory Power Gating Enable
		 * type: RW, rst: 1b, rst domain: DSPLRST
		 *
		 * Register is used to enable the power gating capability of the L2 local
		 * memorySRAM EBB instances. L2 local memory will be power gated for
		 * function disable, or if the DSP subsystem is disabled, independent
		 * of these enable bits.
		 */
		uint8_t  l2lmpge :  1;
		/**
		 * Reserved
		 * type: RO, rst: 00h, rst domain: nan
		 */
		uint8_t   rsvd31 :  7;
	} part;
};

union HSxRMCTL {
	uint8_t full;
	struct {
		/**
		 * L2 Local Memory Retention Mode Enable
		 * type: RW, rst: 1b, rst domain: DSPLRST
		 *
		 * Register is used to enable the retention mode capability of the L2 local
		 * memory SRAM EBB instance.
		 */
		uint8_t  l2lmrme :  1;
		/**
		 * Reserved
		 * type: RO, rst: 00h, rst domain: nan
		 */
		uint8_t   rsvd31 :  7;
	} part;
};

union HSxPGISTS {
	uint8_t full;
	struct {
		/**
		 * L2 Local Memory Power Gating & Initialization Status
		 * type: RO/V, rst: 1b, rst domain: nan
		 *
		 * Register is used to report the current power gating state of the L2 local
		 * memory SRAM EBB instance.
		 * 0: Power on (and initialized)
		 * 1: Power gated
		 */
		uint8_t l2lmpgis :  1;
		/**
		 * Reserved
		 * type: RO, rst: 00h, rst domain: nan
		 */
		uint8_t   rsvd31 :  7;
	} part;
};

struct HPSRAM_REGS
{
	union HSxPGCTL power_gating_control;
	union HSxRMCTL retention_mode_control;
	uint8_t reserved[2];
	union HSxPGISTS power_gating_status;
	uint8_t reserved1[3];
} __packed;

/* end of MTL ACE memory register defines */

#endif
#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */

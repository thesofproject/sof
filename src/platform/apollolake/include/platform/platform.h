/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Xiuli Pan <xiuli.pan@linux.intel.com>
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#define PLATFORM_RESET_MHE_AT_BOOT		1

#define PLATFORM_DISABLE_L2CACHE_AT_BOOT	1

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <rtos/interrupt.h>
#include <rtos/clk.h>
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

/* Host page size */
#define HOST_PAGE_SIZE		4096
#define PLATFORM_PAGE_TABLE_SIZE	256

/* IDC Interrupt */
#define PLATFORM_IDC_INTERRUPT		IRQ_EXT_IDC_LVL2
#define PLATFORM_IDC_INTERRUPT_NAME	irq_name_level2

/* IPC Interrupt */
#define PLATFORM_IPC_INTERRUPT		IRQ_BIT_LVL2_HOST_IPC
#define PLATFORM_IPC_INTERRUPT_NAME	irq_name_level2

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

/* platform has DMA memory type */
#define PLATFORM_MEM_HAS_DMA

/* platform has low power memory type */
#define PLATFORM_MEM_HAS_LP_RAM

/* DSP default delay in cycles */
#define PLATFORM_DEFAULT_DELAY	12

/* minimal L1 exit time in cycles */
#define PLATFORM_FORCE_L1_EXIT_TIME	585

/* the SSP port fifo depth */
#define SSP_FIFO_DEPTH		16

/* the watermark for the SSP fifo depth setting */
#define SSP_FIFO_WATERMARK	8

/* minimal SSP port delay in cycles */
#define PLATFORM_SSP_DELAY	800

/* Platform defined panic code */
static inline void platform_panic(uint32_t p)
{
	mailbox_sw_reg_write(SRAM_REG_FW_STATUS, p & 0x3fffffff);
	ipc_write(IPC_DIPCIE, MAILBOX_EXCEPTION_OFFSET + 2 * 0x20000);
	ipc_write(IPC_DIPCI, 0x80000000 | (p & 0x3fffffff));
}

/**
 * \brief Platform specific CPU entering idle.
 * May be power-optimized using platform specific capabilities.
 * @param level Interrupt level.
 */
void platform_wait_for_interrupt(int level);

extern intptr_t _module_init_start;
extern intptr_t _module_init_end;

#endif
#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */

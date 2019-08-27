/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <sof/lib/memory.h> // Required by below, below is broken
#include <sof/drivers/mu.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/clk.h>
#include <sof/lib/mailbox.h>
#include <stddef.h>
#include <stdint.h>

struct timer;

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)
#define LPSRAM_SIZE 16384

#define PLATFORM_WORKQ_DEFAULT_TIMEOUT	1000

/* IPC Interrupt */
#define PLATFORM_IPC_INTERRUPT		IRQ_NUM_MU
#define PLATFORM_IPC_INTERRUPT_NAME	NULL

/* Host page size */
#define HOST_PAGE_SIZE		4096
#define PLATFORM_PAGE_TABLE_SIZE	256

/* pipeline IRQ */
#define PLATFORM_SCHEDULE_IRQ		IRQ_NUM_SOFTWARE0
#define PLATFORM_SCHEDULE_IRQ_NAME	NULL

#define PLATFORM_SCHEDULE_COST	200

/* maximum preload pipeline depth */
#define MAX_PRELOAD_SIZE	20

/* DMA treats PHY addresses as host address unless within DSP region */
#define PLATFORM_HOST_DMA_MASK	0xFF000000

/* Platform stream capabilities */
#define PLATFORM_MAX_CHANNELS	4
#define PLATFORM_MAX_STREAMS	5

/* clock source used by scheduler for deadline calculations */
#define PLATFORM_SCHED_CLOCK	PLATFORM_DEFAULT_CLOCK

/* DMA channel drain timeout in microseconds - TODO: caclulate based on topology */
#define PLATFORM_DMA_TIMEOUT	1333

/* DMA host transfer timeouts in microseconds */
#define PLATFORM_HOST_DMA_TIMEOUT	50

/* WorkQ window size in microseconds */
#define PLATFORM_WORKQ_WINDOW	2000

/* platform WorkQ clock */
#define PLATFORM_WORKQ_CLOCK	PLATFORM_DEFAULT_CLOCK

/* local buffer size of DMA tracing */
#define DMA_TRACE_LOCAL_SIZE	HOST_PAGE_SIZE

/* trace bytes flushed during panic */
#define DMA_FLUSH_TRACE_SIZE	(MAILBOX_TRACE_SIZE >> 2)

/* the interval of DMA trace copying */
#define DMA_TRACE_PERIOD		500000

/*
 * the interval of reschedule DMA trace copying in special case like half
 * fullness of local DMA trace buffer
 */
#define DMA_TRACE_RESCHEDULE_TIME	100

/* DSP should be idle in this time frame */
#define PLATFORM_IDLE_TIME	750000

/* DSP default delay in cycles */
#define PLATFORM_DEFAULT_DELAY	12

int __dsp_printf(char *format, ...);

/* Platform defined panic code */
static inline void platform_panic(uint32_t p)
{
	__dsp_printf("Platform panic, p = %x\n", p);
	imx_mu_xcr_rmw(IMX_MU_xCR_GIRn(1), 0);
}

/* Platform defined trace code */
#define platform_trace_point(__x)

extern struct timer *platform_timer;

extern intptr_t _module_init_start;
extern intptr_t _module_init_end;
#endif

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */

/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 AMD.All rights reserved.
 *
 * Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *		Anup Kulkarni <anup.kulkarni@amd.com>
 *		Bala Kishore <balakishore.pati@amd.com>
 */
#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <arch/lib/wait.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/clk.h>
#include <sof/lib/mailbox.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/fw_scratch_mem.h>

struct ll_schedule_domain;
struct timer;

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

/* IPC Interrupt */
#define PLATFORM_IPC_INTERRUPT		IRQ_EXT_IPC_LEVEL_3
#define PLATFORM_IPC_INTERRUPT_NAME	NULL

/* Host page size */
#define HOST_PAGE_SIZE                  65536

/* pipeline IRQ */
#define PLATFORM_SCHEDULE_IRQ		IRQ_NUM_SOFTWARE0
#define PLATFORM_SCHEDULE_IRQ_NAME	NULL

/* Platform stream capabilities */
#define PLATFORM_MAX_CHANNELS	4
#define PLATFORM_MAX_STREAMS	5

/* local buffer size of DMA tracing */
#define DMA_TRACE_LOCAL_SIZE	8192

/* trace bytes flushed during panic */
#define DMA_FLUSH_TRACE_SIZE	(MAILBOX_TRACE_SIZE >> 2)

/* the interval of DMA trace copying */
#define DMA_TRACE_PERIOD		500000

/*
 * the interval of reschedule DMA trace copying in special case like half
 * fullness of local DMA trace buffer
 */
#define DMA_TRACE_RESCHEDULE_TIME	100

/* DSP default delay in cycles */
#define PLATFORM_DEFAULT_DELAY	12

/* default dma trace channel */
#define DMA_TRACE_CHANNEL	7

/* Platform defined panic code */
static inline void platform_panic(uint32_t p)
{
	/* TODO */
}

/*
 * brief Platform specific CPU entering idle.
 * May be power-optimized using platform specific capabilities.
 * @param level Interrupt level.
 */
static inline void platform_wait_for_interrupt(int level)
{
	arch_wait_for_interrupt(level);
}

extern intptr_t _module_init_start;
extern intptr_t _module_init_end;
#endif

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */

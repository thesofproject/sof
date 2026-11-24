/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 AMD.All rights reserved.
 *
 * Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *		Bala Kishore <balakishore.pati@amd.com>
 */
#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <rtos/interrupt.h>
#include <rtos/clk.h>
#include <sof/lib/mailbox.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/fw_scratch_mem.h>
#include <platform/chip_registers.h>

struct ll_schedule_domain;
struct timer;

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

/* IPC Interrupt */
#define PLATFORM_IPC_INTERRUPT		IRQ_EXT_IPC_LEVEL_3

/* Host page size */
#define HOST_PAGE_SIZE                  65536

/* default dma trace channel */
#define DMA_TRACE_CHANNEL	7

/*
 * brief Platform specific CPU entering idle.
 * May be power-optimized using platform specific capabilities.
 * @param level Interrupt level.
 */
void platform_wait_for_interrupt(int level);

extern intptr_t _module_init_start;
extern intptr_t _module_init_end;
#endif

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/kernel/platform.h"

#endif /* __SOF_PLATFORM_H__ */

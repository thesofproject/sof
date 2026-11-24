/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <rtos/clk.h>
#include <sof/lib/mailbox.h>
#include <stddef.h>
#include <stdint.h>

struct ll_schedule_domain;
struct timer;

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

/* IPC Interrupt */
#define PLATFORM_IPC_INTERRUPT		7

/* Host page size */
#define HOST_PAGE_SIZE		4096
#define PLATFORM_PAGE_TABLE_SIZE	256

/* Platform defined panic code */
static inline void platform_panic(uint32_t p)
{
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

#error "This file shouldn't be included from outside of sof/kernel/platform.h"

#endif /* __SOF_PLATFORM_H__ */

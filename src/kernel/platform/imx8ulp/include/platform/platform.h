/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 NXP
 *
 * Author: Zhang Peng <peng.zhang_8@nxp.com>
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <sof/kernel/drivers/mu.h>
#include <sof/lib/mailbox.h>
#include <stddef.h>
#include <stdint.h>

struct ll_schedule_domain;
struct timer;

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

/* IPC Interrupt */
#define PLATFORM_IPC_INTERRUPT		15

/* Host page size */
#define HOST_PAGE_SIZE		4096
#define PLATFORM_PAGE_TABLE_SIZE	256

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

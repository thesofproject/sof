/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#include <arch/lib/wait.h>
#include <sof/lib/clk.h>
#include <stdint.h>

struct timer;

/*! \def PLATFORM_DEFAULT_CLOCK
 *  \brief clock source for audio pipeline
 *
 *  There are two types of clock: cpu clock which is a internal clock in
 *  xtensa core, and ssp clock which is provided by external HW IP.
 *  The choice depends on HW features on different platform
 */
#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

/* Host page size */
#define HOST_PAGE_SIZE		4096

/* Platform stream capabilities */
#define PLATFORM_MAX_CHANNELS	8
#define PLATFORM_MAX_STREAMS	16

/* IPC page data copy timeout */
#define PLATFORM_IPC_DMA_TIMEOUT 2000

/* DSP default delay in cycles */
#define PLATFORM_DEFAULT_DELAY	12

static inline void platform_panic(uint32_t p) {}

/**
 * \brief Platform specific CPU entering idle.
 * May be power-optimized using platform specific capabilities.
 * @param level Interrupt level.
 */
static inline void platform_wait_for_interrupt(int level)
{
	arch_wait_for_interrupt(level);
}

extern struct timer *platform_timer;

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */

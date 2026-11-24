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

#include <rtos/wait.h>
#include <rtos/clk.h>
#include <sof/lib/mailbox.h>
#include <stdint.h>

struct ipc_msg;
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

#define HW_CFG_VERSION		0

/**
 * \brief Platform specific CPU entering idle.
 * May be power-optimized using platform specific capabilities.
 * @param level Interrupt level.
 */
static inline void platform_wait_for_interrupt(int level) {}


static inline int ipc_platform_send_msg(const struct ipc_msg *msg) { return 0; }

static inline void ipc_platform_send_msg_direct(const struct ipc_msg *msg) {}

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/kernel/platform.h"

#endif /* __SOF_PLATFORM_H__ */

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

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <rtos/clk.h>
#include <stddef.h>
#include <stdint.h>

#include <cavs/version.h>

struct ll_schedule_domain;
struct timer;

/* Host page size */
#define HOST_PAGE_SIZE		4096

/* Platform stream capabilities */
#define PLATFORM_MAX_CHANNELS	8
#define PLATFORM_MAX_STREAMS	16

/* local buffer size of DMA tracing */
#define DMA_TRACE_LOCAL_SIZE	(HOST_PAGE_SIZE * 2)

extern intptr_t _module_init_start;
extern intptr_t _module_init_end;

#endif
#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */

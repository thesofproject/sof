/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 *         Xiuli Pan <xiuli.pan@linux.intel.com>
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#include <rtos/clk.h>

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

/* Platform stream capabilities */
#define PLATFORM_MAX_CHANNELS	8
#define PLATFORM_MAX_STREAMS	16

/* local buffer size of DMA tracing */
#define DMA_TRACE_LOCAL_SIZE	(HOST_PAGE_SIZE * 2)

/* DSP default delay in cycles */
#define PLATFORM_DEFAULT_DELAY	12

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */

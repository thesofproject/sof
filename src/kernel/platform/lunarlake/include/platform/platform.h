/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
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

/* Host page size */
#define HOST_PAGE_SIZE		4096
#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/kernel/platform.h"

#endif /* __SOF_PLATFORM_H__ */

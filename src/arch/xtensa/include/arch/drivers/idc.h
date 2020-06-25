/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/lib/cpu.h>
#ifndef __ZEPHYR__
#include <xtos-structs.h>
#endif

struct idc;

#ifndef __ZEPHYR__
/**
 * \brief Returns IDC data.
 * \return Pointer to pointer of IDC data.
 */
static inline struct idc **idc_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->idc;
}

#else

struct idc **idc_get(void);

#endif

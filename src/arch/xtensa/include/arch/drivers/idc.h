/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/lib/cpu.h>
#include <xtos-structs.h>

struct idc;

/**
 * \brief Returns IDC data.
 * \return Pointer to pointer of IDC data.
 */
static inline struct idc **idc_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->idc;
}

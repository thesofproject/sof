/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __XTOS_RTOS_IDC_H__

#ifndef __ARCH_DRIVERS_IDC_H__
#define __ARCH_DRIVERS_IDC_H__

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

#endif /* __ARCH_DRIVERS_IDC_H__ */

#else

#error "This file shouldn't be included from outside of XTOS's rtos/idc.h"

#endif /* __XTOS_RTOS_IDC_H__ */

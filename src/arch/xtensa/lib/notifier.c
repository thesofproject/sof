// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file
 * \brief Xtensa notifier implementation file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/lib/cpu.h>
#include <sof/lib/notifier.h>
#include <xtos-structs.h>

struct notify **arch_notify_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->notify;
}

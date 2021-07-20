// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file
 * \brief Xtensa schedule implementation file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/lib/cpu.h>
#include <sof/schedule/schedule.h>
#include <xtos-structs.h>

struct schedulers **arch_schedulers_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->schedulers;
}

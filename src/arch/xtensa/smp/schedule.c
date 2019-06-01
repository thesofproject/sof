// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file arch/xtensa/smp/schedule.c
 * \brief Xtensa SMP schedule implementation file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <xtos-structs.h>
#include <arch/cpu.h>
#include <sof/schedule.h>

struct schedule_data **arch_schedule_get_data(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->sch_data;
}

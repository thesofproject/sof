// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

/**
 * @file
 * @brief Xtensa IPC per-core data access
 */

#include <sof/ipc/common.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <xtos-structs.h>

struct ipc_core_ctx **arch_ipc_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->ipc;
}

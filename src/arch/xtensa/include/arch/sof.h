/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifdef __SOF_SOF_H__

#ifndef __ARCH_SOF_H__
#define __ARCH_SOF_H__

#include <sof/schedule/task.h>
#include <xtensa/xtruntime-frames.h>
#include <stdint.h>

/* entry point to main firmware */
void _ResetVector(void);

void boot_master_core(void);

static inline void *arch_get_stack_ptr(void)
{
	void *ptr;

	/* stack pointer is in a1 */
	__asm__ __volatile__ ("mov %0, a1"
		: "=a" (ptr)
		:
		: "memory");
	return ptr;
}

static inline void *arch_get_stack_entry(void)
{
	volatile xtos_task_context *task_ctx = task_context_get();

	return task_ctx->stack_base;
}

static inline uint32_t arch_get_stack_size(void)
{
	volatile xtos_task_context *task_ctx = task_context_get();

	return task_ctx->stack_size;
}

#endif /* __ARCH_SOF_H__ */

#else

#error "This file shouldn't be included from outside of sof/sof.h"

#endif /* __SOF_SOF_H__ */

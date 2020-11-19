/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 */

#ifdef __SOF_DEBUG_BACKTRACE_H__

#ifndef __ARCH_DEBUG_BACKTRACE_H__
#define __ARCH_DEBUG_BACKTRACE_H__

#include <sof/schedule/task.h>
#include <xtensa/xtruntime-frames.h>
#include <stdint.h>

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

#endif /* __ARCH_DEBUG_BACKTRACE_H__ */

#else

#error "This file shouldn't be included from outside of sof/debug/backtrace.h"

#endif /* __SOF_DEBUG_BACKTRACE_H__ */

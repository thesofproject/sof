/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __XTOS_XTOS_STRUCTS_H__
#define __XTOS_XTOS_STRUCTS_H__

#include "xtos-internal.h"
#include <sof/common.h>
#include <sof/lib/memory.h>
#include <sof/common.h>

#include <xtensa/xtruntime-frames.h>
#include <stdint.h>

struct idc;
struct notify;
struct schedulers;
struct task;

struct thread_data {
	xtos_structures_pointers xtos_ptrs;
	volatile xtos_task_context *xtos_active_task;
};

struct xtos_core_data {
#if CONFIG_MULTICORE
	struct XtosInterruptStructure xtos_int_data;
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_1
	uint8_t xtos_stack_for_interrupt_1[SOF_STACK_SIZE] __aligned(16);
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_2
	uint8_t xtos_stack_for_interrupt_2[SOF_STACK_SIZE] __aligned(16);
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_3
	uint8_t xtos_stack_for_interrupt_3[SOF_STACK_SIZE] __aligned(16);
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_4
	uint8_t xtos_stack_for_interrupt_4[SOF_STACK_SIZE] __aligned(16);
#endif
#if CONFIG_XT_INTERRUPT_LEVEL_5
	uint8_t xtos_stack_for_interrupt_5[SOF_STACK_SIZE] __aligned(16);
#endif
	xtos_task_context xtos_interrupt_ctx;
	uintptr_t xtos_saved_sp;
	struct thread_data *thread_data_ptr;
};

STATIC_ASSERT((sizeof(struct xtos_core_data) % (PLATFORM_DCACHE_ALIGN)) == 0,
		xtos_core_data_payload_must_be_aligned_to_cache_line_size);

struct core_context {
	union {
		struct {
			struct thread_data td;
			struct task *main_task;
			struct schedulers *schedulers;
			struct notify *notify;
			struct idc *idc;
		};
		uint8_t __cache_alignment[PLATFORM_DCACHE_ALIGN];
	};
} __attribute__((aligned(PLATFORM_DCACHE_ALIGN)));

#endif /* __XTOS_XTOS_STRUCTS_H__ */

// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/**
 * \file arch/xtensa/init.c
 * \brief Xtensa initialization functions
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include "xtos-internal.h"
#include <sof/common.h>
#include <sof/init.h>
#include <sof/lib/cpu.h>
#include <sof/sof.h>
#include <sof/spinlock.h>

#include <xtensa/xtruntime-frames.h>
#include <xtos-structs.h>
#include <stddef.h>
#include <stdint.h>

/* UserFrame's size needs to be 16 bytes aligned */
STATIC_ASSERT((sizeof(UserFrame) % 16) == 0, invalid_UserFrame_alignment);

/* verify xtos_active_task offset */
STATIC_ASSERT(offsetof(struct thread_data, xtos_active_task) ==
	      XTOS_TASK_CONTEXT_OFFSET, invalid_xtos_active_task_offset);

#if CONFIG_DEBUG_LOCKS
/** \brief Debug lock. */
uint32_t lock_dbg_atomic;

/** \brief Debug locks per user. */
uint32_t lock_dbg_user[DBG_LOCK_USERS] = {0};
#endif
#if CONFIG_NO_SECONDARY_CORE_ROM
void *shared_vecbase_ptr __aligned(PLATFORM_DCACHE_ALIGN);
#endif
/** \brief Core context for primary core. */
static struct core_context primary_core_ctx;

/** \brief Core context pointers for all the cores. */
struct core_context *core_ctx_ptr[CONFIG_CORE_COUNT] = { 0 };

/** \brief Xtos core data for primary core. */
struct xtos_core_data primary_core_data;

/** \brief Xtos core data pointers for all the cores. */
struct xtos_core_data *core_data_ptr[CONFIG_CORE_COUNT] = { 0 };

/**
 * \brief Initializes core specific data.
 */
static void initialize_pointers_per_core(void)
{
	int core = cpu_get_id();
	struct xtos_core_data *core_data;
	xtos_structures_pointers *p;

	if (core == PLATFORM_PRIMARY_CORE_ID) {
		primary_core_data.thread_data_ptr = &primary_core_ctx.td;
		core_ctx_ptr[PLATFORM_PRIMARY_CORE_ID] = &primary_core_ctx;
		core_data_ptr[PLATFORM_PRIMARY_CORE_ID] = &primary_core_data;
	}

	cpu_write_threadptr((int)core_ctx_ptr[core]);

	core_data = core_data_ptr[core];

	p = &core_data->thread_data_ptr->xtos_ptrs;
	p->xtos_interrupt_ctx = &core_data->xtos_interrupt_ctx;
	p->xtos_saved_sp = &core_data->xtos_saved_sp;
#if CONFIG_INTERRUPT_LEVEL_1
	p->xtos_stack_for_interrupt_1 = core_data->xtos_stack_for_interrupt_1;
#endif
#if CONFIG_INTERRUPT_LEVEL_2
	p->xtos_stack_for_interrupt_2 = core_data->xtos_stack_for_interrupt_2;
#endif
#if CONFIG_INTERRUPT_LEVEL_3
	p->xtos_stack_for_interrupt_3 = core_data->xtos_stack_for_interrupt_3;
#endif
#if CONFIG_INTERRUPT_LEVEL_4
	p->xtos_stack_for_interrupt_4 = core_data->xtos_stack_for_interrupt_4;
#endif
#if CONFIG_INTERRUPT_LEVEL_5
	p->xtos_stack_for_interrupt_5 = core_data->xtos_stack_for_interrupt_5;
#endif
#if CONFIG_MULTICORE
	p->xtos_enabled = &core_data->xtos_int_data.xtos_enabled;
	p->xtos_intstruct = &core_data->xtos_int_data;
	p->xtos_interrupt_table =
		&core_data->xtos_int_data.xtos_interrupt_table.array[0];
	p->xtos_interrupt_mask_table =
		&core_data->xtos_int_data.xtos_interrupt_mask_table[0];
#endif
}

/**
 * \brief Initializes architecture.
 * \return Error status.
 */
int arch_init(void)
{
	initialize_pointers_per_core();
	register_exceptions();
	return 0;
}

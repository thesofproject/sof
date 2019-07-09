/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file arch/xtensa/smp/include/arch/alloc.h
 * \brief Xtensa SMP memory allocation header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __ARCH_ALLOC_H__
#define __ARCH_ALLOC_H__

#include <xtos-structs.h>
#include <platform/cpu.h>
#include <sof/alloc.h>

extern struct core_context *core_ctx_ptr[PLATFORM_CORE_COUNT];
extern struct xtos_core_data *core_data_ptr[PLATFORM_CORE_COUNT];
extern unsigned int _bss_start, _bss_end;

/**
 * \brief Allocates memory for core specific data.
 * \param[in] core Slave core for which data needs to be allocated.
 */
static inline void alloc_core_context(int core)
{
	struct core_context *core_ctx;

	core_ctx = rzalloc_core_sys(core, sizeof(*core_ctx));
	dcache_writeback_invalidate_region(core_ctx, sizeof(*core_ctx));

	core_data_ptr[core] = rzalloc_core_sys(core,
					       sizeof(*core_data_ptr[core]));
	core_data_ptr[core]->thread_data_ptr = &core_ctx->td;
	dcache_writeback_invalidate_region(core_data_ptr[core],
					   sizeof(*core_data_ptr[core]));

	dcache_writeback_invalidate_region(core_data_ptr,
					   sizeof(core_data_ptr));

	core_ctx_ptr[core] = core_ctx;
	dcache_writeback_invalidate_region(core_ctx_ptr,
					   sizeof(core_ctx_ptr));

	/* writeback bss region to share static pointers */
	dcache_writeback_region((void *)&_bss_start,
				(unsigned int)&_bss_end -
				(unsigned int)&_bss_start);
}

#endif

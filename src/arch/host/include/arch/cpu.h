/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Rander Wang <rander.wang@linux.intel.com>
 */

#ifndef __INCLUDE_ARCH_CPU__
#define __INCLUDE_ARCH_CPU__

static inline void arch_cpu_enable_core(int id)
{
}

static inline void arch_cpu_disable_core(int id)
{
}

static inline int arch_cpu_is_core_enabled(int id)
{
	return 0;
}

static inline int arch_cpu_get_id(void)
{
	return 0;
}

static inline void cpu_write_threadptr(int threadptr)
{
}

static inline int cpu_read_threadptr(void)
{
	return 0;
}

#endif

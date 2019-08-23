/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file include/sof/cpu.h
 * \brief CPU header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_CPU_H__
#define __SOF_CPU_H__

#include "../arch/cpu.h"

static inline int cpu_get_id(void)
{
	return arch_cpu_get_id();
}

static inline void cpu_enable_core(int id)
{
}

static inline void cpu_disable_core(int id)
{
}

static inline int cpu_is_core_enabled(int id)
{
	return 0;
}

#endif

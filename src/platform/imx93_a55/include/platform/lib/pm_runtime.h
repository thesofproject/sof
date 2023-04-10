/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifdef __SOF_LIB_PM_RUNTIME_H__

#ifndef __PLATFORM_LIB_PM_RUNTIME_H__
#define __PLATFORM_LIB_PM_RUNTIME_H__

/* TODO: is this required? Should this be implemented using Zephyr PM
 * API?
 */
#include <stdint.h>

struct pm_runtime_data;

static inline void platform_pm_runtime_init(struct pm_runtime_data *prd)
{
}

static inline void platform_pm_runtime_get(uint32_t context,
					   uint32_t index,
					   uint32_t flags)
{
}

static inline void platform_pm_runtime_put(uint32_t context,
					   uint32_t index,
					   uint32_t flags)
{
}

static inline void platform_pm_runtime_enable(uint32_t context,
					      uint32_t index)
{
}

static inline void platform_pm_runtime_disable(uint32_t context,
					       uint32_t index)
{
}

static inline bool platform_pm_runtime_is_active(uint32_t context,
						 uint32_t index)
{
	return false;
}

#endif /* __PLATFORM_LIB_PM_RUNTIME_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/pm_runtime.h"

#endif /* __SOF_LIB_PM_RUNTIME_H__ */

/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef _SOF_PLATFORM_MTK_LIB_PM_RUNTIME_H
#define _SOF_PLATFORM_MTK_LIB_PM_RUNTIME_H

#include <stdint.h>

struct pm_runtime_data;

static inline void platform_pm_runtime_init(struct pm_runtime_data *prd)
{
}

static inline void platform_pm_runtime_get(uint32_t context, uint32_t index, uint32_t flags)
{
}

static inline void platform_pm_runtime_put(uint32_t context, uint32_t index, uint32_t flags)
{
}

static inline void platform_pm_runtime_enable(uint32_t context, uint32_t index)
{
}

static inline void platform_pm_runtime_disable(uint32_t context, uint32_t index)
{
}

static inline bool platform_pm_runtime_is_active(uint32_t context, uint32_t index)
{
	return false;
}

#endif /* _SOF_PLATFORM_MTK_LIB_PM_RUNTIME_H */

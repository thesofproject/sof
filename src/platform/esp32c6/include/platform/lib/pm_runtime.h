/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Espressif Systems / SOF Project
 */

#ifndef __PLATFORM_LIB_PM_RUNTIME_H__
#define __PLATFORM_LIB_PM_RUNTIME_H__

struct pm_runtime_data;

static inline void platform_pm_runtime_init(struct pm_runtime_data *prd) {}
static inline void platform_pm_runtime_get(struct pm_runtime_data *prd) {}
static inline void platform_pm_runtime_put(struct pm_runtime_data *prd) {}

#endif /* __PLATFORM_LIB_PM_RUNTIME_H__ */

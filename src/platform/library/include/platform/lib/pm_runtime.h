/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 */

#ifdef __SOF_LIB_PM_RUNTIME_H__

#ifndef __PLATFORM_LIB_PM_RUNTIME_H__
#define __PLATFORM_LIB_PM_RUNTIME_H__

static inline void platform_pm_runtime_init(struct pm_runtime_data *prd) {}
static inline void platform_pm_runtime_get(uint32_t context, uint32_t index, uint32_t flags) {}
static inline void platform_pm_runtime_put(uint32_t context, uint32_t index, uint32_t flags) {}
static inline void platform_pm_runtime_enable(uint32_t context, uint32_t index) {}
static inline void platform_pm_runtime_disable(uint32_t context, uint32_t index) {}
static inline bool platform_pm_runtime_is_active(uint32_t context, uint32_t index) { return false; }

#endif

#endif

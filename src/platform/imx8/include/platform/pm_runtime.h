/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __INCLUDE_PLATFORM_PM_RUNTIME__
#define __INCLUDE_PLATFORM_PM_RUNTIME__

#include <sof/pm_runtime.h>

/**
 * \brief Initializes platform specific runtime power management.
 * \param[in,out] prd Runtime power management data.
 */
static inline void platform_pm_runtime_init(struct pm_runtime_data *prd) { }

/**
 * \brief Retrieves platform specific power management resource.
 *
 * \param[in] context Type of power management context.
 * \param[in] index Index of the device.
 * \param[in] flags Flags, set of RPM_...
 */
static inline void platform_pm_runtime_get(enum pm_runtime_context context,
					   uint32_t index, uint32_t flags) { }

/**
 * \brief Releases platform specific power management resource.
 *
 * \param[in] context Type of power management context.
 * \param[in] index Index of the device.
 * \param[in] flags Flags, set of RPM_...
 */
static inline void platform_pm_runtime_put(enum pm_runtime_context context,
					   uint32_t index, uint32_t flags) { }

#endif /* __INCLUDE_PLATFORM_PM_RUNTIME__ */

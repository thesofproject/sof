/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

/**
 * \file include/sof/lib/pm_runtime.h
 * \brief Runtime power management header file
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_LIB_PM_RUNTIME_H__
#define __SOF_LIB_PM_RUNTIME_H__

#include <platform/lib/pm_runtime.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

/** \addtogroup pm_runtime PM Runtime
 *  PM runtime specification.
 *  @{
 */

/** \brief Power management trace function. */
#define trace_pm(__e, ...) \
	trace_event(TRACE_CLASS_POWER, __e, ##__VA_ARGS__)
#define tracev_pm(__e, ...) \
	tracev_event(TRACE_CLASS_POWER, __e, ##__VA_ARGS__)

/* PM runtime flags */

#define RPM_ASYNC		0x01	/**< Request is asynchronous */

/** \brief Runtime power management context */
enum pm_runtime_context {
	PM_RUNTIME_HOST_DMA_L1 = 0,	/**< Host DMA L1 Exit */
	SSP_CLK,			/**< SSP Clock */
	SSP_POW,			/**< SSP Power */
	DMIC_CLK,			/**< DMIC Clock */
	DMIC_POW,			/**< DMIC Power */
	DW_DMAC_CLK,			/**< DW DMAC Clock */
	CORE_MEMORY_POW,		/**< Core Memory power */
	PM_RUNTIME_DSP			/**< DSP */
};

/** \brief Runtime power management data. */
struct pm_runtime_data {
	spinlock_t lock;	/**< lock mechanism */
	void *platform_data;	/**< platform specific data */
};

/**
 * \brief Initializes runtime power management.
 */
void pm_runtime_init(struct sof *sof);

/**
 * \brief Retrieves power management resource (async).
 *
 * \param[in] context Type of power management context.
 * \param[in] index Index of the device.
 */
void pm_runtime_get(enum pm_runtime_context context, uint32_t index);

/**
 * \brief Retrieves power management resource.
 *
 * \param[in] context Type of power management context.
 * \param[in] index Index of the device.
 */
void pm_runtime_get_sync(enum pm_runtime_context context, uint32_t index);

/**
 * \brief Releases power management resource (async).
 *
 * \param[in] context Type of power management context.
 * \param[in] index Index of the device.
 */
void pm_runtime_put(enum pm_runtime_context context, uint32_t index);

/**
 * \brief Releases power management resource.
 *
 * \param[in] context Type of power management context.
 * \param[in] index Index of the device.
 */
void pm_runtime_put_sync(enum pm_runtime_context context, uint32_t index);

/**
 * \brief Enables power management operations for the resource.
 *
 * \param[in] context Type of power management context.
 * \param[in] index Index of the device.
 */
void pm_runtime_enable(enum pm_runtime_context context, uint32_t index);

/**
 * \brief Disables power management operations for the resource.
 *
 * \param[in] context Type of power management context.
 * \param[in] index Index of the device.
 */
void pm_runtime_disable(enum pm_runtime_context context, uint32_t index);

/**
 * \brief Reports state of the power managed resource.
 *
 * @param context Type of power management context.
 * @param index Index of the resource.
 *
 * @return true if the resource is active or pm disabled, false otherwise.
 */
bool pm_runtime_is_active(enum pm_runtime_context context, uint32_t index);

/**
 * \brief Retrieves pointer to runtime power management data.
 *
 * @return Runtime power management data pointer.
 */
static inline struct pm_runtime_data *pm_runtime_data_get(void)
{
	return sof_get()->prd;
}

/** @}*/

#endif /* __SOF_LIB_PM_RUNTIME_H__ */

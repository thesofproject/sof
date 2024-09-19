/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOF_LIB_PM_RUNTIME_H__
#define __SOF_LIB_PM_RUNTIME_H__

#include <rtos/sof.h>
#include <stdint.h>

/** \addtogroup pm_runtime PM Runtime
 *  SOF PM runtime specification mapping for Zephyr builds.
 *
 *  This interface is considered deprecated and native Zephyr
 *  interfaces should be used instead.
 *  @{
 */

/** \brief Runtime power management context */
enum pm_runtime_context {
	PM_RUNTIME_DSP			/**< DSP */
};

/**
 * \brief Initializes runtime power management.
 */
static inline void pm_runtime_init(struct sof *sof)
{
}

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
static inline void pm_runtime_get_sync(enum pm_runtime_context context, uint32_t index)
{
}


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
static inline void pm_runtime_put_sync(enum pm_runtime_context context, uint32_t index)
{
}

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

static inline void platform_pm_runtime_prepare_d0ix_en(uint32_t index)
{
}

#if CONFIG_DSP_RESIDENCY_COUNTERS

/**
 * \brief Initializes DSP residency counters.
 *
 * \param[in] context Type of power management context.
 */
static inline void init_dsp_r_state(enum dsp_r_state)
{
}

/**
 * \brief Reports DSP residency state.
 *
 * \param[in] new state
 */
static inline void report_dsp_r_state(enum dsp_r_state)
{
}

/**
 * \brief Retrieves current DSP residency state.
 *
 * @return active DSP residency state
 */
static inline enum dsp_r_state get_dsp_r_state(void)
{
}
#endif

/** @}*/

#endif /* __SOF_LIB_PM_RUNTIME_H__ */

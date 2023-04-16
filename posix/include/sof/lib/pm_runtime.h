/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

/**
 * \file xtos/include/sof/lib/pm_runtime.h
 * \brief Runtime power management header file
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_LIB_PM_RUNTIME_H__
#define __SOF_LIB_PM_RUNTIME_H__

#include <platform/lib/pm_runtime.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

/** \addtogroup pm_runtime PM Runtime
 *  PM runtime specification.
 *  @{
 */

/* PM runtime flags */

#define RPM_ASYNC		0x01	/**< Request is asynchronous */

/** \brief Runtime power management context */
enum pm_runtime_context {
	PM_RUNTIME_HOST_DMA_L1 = 0,	/**< Host DMA L1 */
	SSP_CLK,			/**< SSP Clock */
	SSP_POW,			/**< SSP Power */
	DMIC_CLK,			/**< DMIC Clock */
	DMIC_POW,			/**< DMIC Power */
	DW_DMAC_CLK,			/**< DW DMAC Clock */
	CORE_MEMORY_POW,		/**< Core Memory power */
	CORE_HP_CLK,			/**< High Performance Clock*/
	PM_RUNTIME_DSP			/**< DSP */
};

/** \brief Runtime power management data. */
struct pm_runtime_data {
	struct k_spinlock lock;	/**< lock mechanism */
	void *platform_data;	/**< platform specific data */
#if CONFIG_DSP_RESIDENCY_COUNTERS
	struct r_counters_data *r_counters; /**< diagnostic DSP residency counters */
#endif
};

#if CONFIG_DSP_RESIDENCY_COUNTERS
/**
 * \brief DSP residency counters
 * R0, R1, R2 are DSP residency counters which can be used differently
 * based on platform implementation.
 * In general R0 is the highest power consumption state while R2 is
 * the lowest power consumption state. See platform specific pm_runtime.h
 * for the platform HW specific mapping.
 */
enum dsp_r_state {
	r0_r_state = 0,
	r1_r_state,
	r2_r_state
};

/** \brief Diagnostic DSP residency counters data */
struct r_counters_data {
	enum dsp_r_state cur_r_state;	/**< current dsp_r_state */
	uint64_t ts;					/**< dsp_r_state timestamp */
};
#endif

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

#if CONFIG_DSP_RESIDENCY_COUNTERS
/**
 * \brief Initializes DSP residency counters.
 *
 * \param[in] context Type of power management context.
 */
void init_dsp_r_state(enum dsp_r_state);

/**
 * \brief Reports DSP residency state.
 *
 * \param[in] new state
 */
void report_dsp_r_state(enum dsp_r_state);

/**
 * \brief Retrieves current DSP residency state.
 *
 * @return active DSP residency state
 */
enum dsp_r_state get_dsp_r_state(void);
#endif

/** @}*/

#endif /* __SOF_LIB_PM_RUNTIME_H__ */

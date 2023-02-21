/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Krzysztof Frydryk <frydryk.krzysztof@intel.com>
 */

/**
 * \file include/sof/lib/cpu-clk-manager.h
 * \brief CPU clk manager definition
 * \author Krzysztof Frydryk <krzysztofx.frydryk@intel.com>
 */

#ifndef __SOF_LIB_CPU_CLK_MANAGER_H__
#define __SOF_LIB_CPU_CLK_MANAGER_H__

#include <rtos/spinlock.h>
#include <rtos/sof.h>
#include <stdint.h>

/**
 * \brief CPS budget data.
 */
struct kcps_budget_data {
	/* uncache only */
	int kcps_consumption[CONFIG_CORE_COUNT];	/* Sum of declared consumptions on core */
	struct k_spinlock lock;
};

/**
 * \brief Declare KCPS usage on core
 *
 * Declare KCPS (kilo cycles per second) CPU usage on core and adjust
 * CPU clock according to sum of declared consumptions.
 *
 * Declared consumption can be negative in case of freeing CPS.
 *
 * @param core The core to which consumption should be pinned
 * @param kcps_delta declared usage. Can be negative.
 */
int core_kcps_adjust(int core, int kcps_delta);

/**
 * \brief Get KCPS usage on core
 *
 * Get declared KCPS usage on core
 *
 * @param core The core which consumption should be returned
 */
int core_kcps_get(int core);

/**
 * \brief Init KCPS budget mechanism
 */
int kcps_budget_init(void);

#endif /*__SOF_LIB_CPU_CLK_MANAGER_H__ */

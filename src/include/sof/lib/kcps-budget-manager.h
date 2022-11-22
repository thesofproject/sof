/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Krzysztof Frydryk <frydryk.krzysztof@intel.com>
 */

#ifndef __SOF_LIB_CPS_BUDGET_MANAGER_H__
#define __SOF_LIB_CPS_BUDGET_MANAGER_H__

#include <rtos/spinlock.h>
#include <sof/sof.h>
#include <stdint.h>

struct kcps_budget_data {
	/* uncache only */
	int kcps_consumption[CONFIG_CORE_COUNT];
	struct k_spinlock lock;
};

int core_kcps_adjust(int core, int kcps_delta);
int core_kcps_get(int core);

int kcps_budget_init(void);

#endif /*__SOF_LIB_CPS_BUDGET_MANAGER_H__ */

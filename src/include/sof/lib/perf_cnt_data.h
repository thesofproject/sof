/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

/**
 * \file include/sof/lib/perf_cnt_data.h
 * \brief Performance counter data, without the timer based helpers
 *
 * Kept separate from sof/lib/perf_cnt.h so that structures embedding the
 * counters (tasks, components) do not depend on the timer headers.
 */

#ifndef __SOF_LIB_PERF_CNT_DATA_H__
#define __SOF_LIB_PERF_CNT_DATA_H__

#include <stdint.h>

struct perf_cnt_data {
	/* current platform clock */
	uint32_t plat_ts;
	/* current cpu clock */
	uint32_t cpu_ts;
	/* one period module cycles */
	uint32_t plat_delta_last;
	/* period module peak cycles within period_cnt */
	uint32_t plat_delta_peak;
	/* one period module cycles from cpu clock */
	uint32_t cpu_delta_last;
	/* period module peak cycles within period_cnt from cpu clock */
	uint32_t cpu_delta_peak;
	/* total cycles within period_cnt */
	uint32_t cpu_delta_sum;
	/* accumulate period passed and calculate avg cycles */
	uint32_t period_cnt;
	/* indicate which period have peak mcps in current module */
	uint32_t peak_mcps_period_cnt;
};

#endif /* __SOF_LIB_PERF_CNT_DATA_H__ */

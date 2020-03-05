/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

/**
 * \file include/sof/lib/perf_cnt.h
 * \brief Simple performance counters
 * \author Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __SOF_LIB_PERF_CNT_H__
#define __SOF_LIB_PERF_CNT_H__

#include <sof/drivers/timer.h>

struct perf_cnt_data {
	uint64_t plat_ts;
	uint64_t cpu_ts;
	uint64_t plat_delta_last;
	uint64_t plat_delta_peak;
	uint64_t cpu_delta_last;
	uint64_t cpu_delta_peak;
};

#if CONFIG_PERFORMANCE_COUNTERS

#define perf_cnt_trace(tclass, pcd) \
		trace_event(tclass, "perf plat last %lu peak %lu cpu last %lu, peak %lu", \
			    (pcd)->plat_delta_last,	\
			    (pcd)->plat_delta_peak,	\
			    (pcd)->cpu_delta_last,	\
			    (pcd)->cpu_delta_peak)

/** \brief Clears performance counters data. */
#define perf_cnt_clear(pcd) memset((pcd), 0, sizeof(struct perf_cnt_data))

/** \brief Initializes timestamps with current timer values. */
#define perf_cnt_init(pcd) do {						\
		(pcd)->plat_ts = platform_timer_get(timer_get());	\
		(pcd)->cpu_ts = arch_timer_get_system(cpu_timer_get());	\
	} while (0)

/* Trace macros that can be used as trace_m argument of the perf_cnt_stamp()
 * to trace PCD values if the last arch timer reading exceeds the previous
 * peak value.
 *
 * arg passed to perf_cnt_stamp() is forwarded to the trace_m() macro
 * as the second argument.
 */

/** \brief No trace when detecting peak value. */
#define perf_trace_null(pcd, arg)

/** \brief Simple trace, all values are printed, arg should be one
 *         of TRACE_CLASS_...
 */
#define perf_trace_simple(pcd, arg) perf_cnt_trace(arg, pcd)

/** \brief Reads the timers and computes delta to the previous readings.
 *
 *  If current arch delta exceeds the previous peak value, trace_m is run.
 *  \param pcd Performance counters data.
 *  \param trace_m Trace macro trace_m(pcd, arg).
 *  \param arg Argument passed to trace_m as arg.
 */
#define perf_cnt_stamp(pcd, trace_m, arg) do {				  \
		uint64_t plat_ts = platform_timer_get(timer_get());	  \
		uint64_t cpu_ts = arch_timer_get_system(cpu_timer_get()); \
		if ((pcd)->plat_ts) {					  \
			(pcd)->plat_delta_last = plat_ts - (pcd)->plat_ts;\
			(pcd)->cpu_delta_last = cpu_ts - (pcd)->cpu_ts;   \
		}							  \
		(pcd)->plat_ts = plat_ts;				  \
		(pcd)->cpu_ts = cpu_ts;					  \
		if ((pcd)->plat_delta_last > (pcd)->plat_delta_peak)	  \
			(pcd)->plat_delta_peak = (pcd)->plat_delta_last;  \
		if ((pcd)->cpu_delta_last > (pcd)->cpu_delta_peak) {	  \
			(pcd)->cpu_delta_peak = (pcd)->cpu_delta_last;	  \
			trace_m(pcd, arg);				  \
		}							  \
	} while (0)

#else
#define perf_cnt_clear(pcd)
#define perf_cnt_init(pcd)
#define perf_cnt_stamp(pcd, trace_m, arg)
#endif

#endif /* __SOF_LIB_PERF_CNT_H__ */

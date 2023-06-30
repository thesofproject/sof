/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

/**
 * \file xtos/include/sof/lib/perf_cnt.h
 * \brief Simple performance counters
 * \author Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __SOF_LIB_PERF_CNT_H__
#define __SOF_LIB_PERF_CNT_H__

#include <rtos/timer.h>

struct perf_cnt_data {
	uint32_t plat_ts;
	uint32_t cpu_ts;
	uint32_t plat_delta_last;
	uint32_t plat_delta_peak;
	uint32_t cpu_delta_last;
	uint32_t cpu_delta_peak;
	uint32_t cpu_delta_sum;
	uint32_t sample_cnt;
};

#if CONFIG_PERFORMANCE_COUNTERS

#define perf_cnt_trace(ctx, pcd) \
		tr_info(ctx, "perf plat last %u peak %u cpu last %u, peak %u", \
			(uint32_t)((pcd)->plat_delta_last),	\
			(uint32_t)((pcd)->plat_delta_peak),	\
			(uint32_t)((pcd)->cpu_delta_last),	\
			(uint32_t)((pcd)->cpu_delta_peak))

/** \brief Clears performance counters data. */
#define perf_cnt_clear(pcd) memset((pcd), 0, sizeof(struct perf_cnt_data))

/* NOTE: Zephyr's arch_timing_counter_get() might not be implemented
 * for a particular platform. In this case let's fallback to use
 * Zephyr's k_cycle_get_64(). This will result in both "platform" and
 * "cpu" timestamps to be equal.
 */
#ifdef __ZEPHYR__
	#ifdef CONFIG_TIMING_FUNCTIONS
		#define perf_cnt_get_cpu_ts arch_timing_counter_get
	#else
		#define perf_cnt_get_cpu_ts sof_cycle_get_64
	#endif	/* CONFIG_TIMING_FUNCTIONS */
#else
	#define perf_cnt_get_cpu_ts() timer_get_system(cpu_timer_get())
#endif	/* __ZEPHYR__ */

/** \brief Initializes timestamps with current timer values. */
#define perf_cnt_init(pcd) do {						\
		(pcd)->plat_ts = sof_cycle_get_64();			\
		(pcd)->cpu_ts = perf_cnt_get_cpu_ts();			\
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

/** \brief Simple trace, all values are printed, arg should be a tr_ctx address.
 */
#define perf_trace_simple(pcd, arg) perf_cnt_trace(arg, pcd)

/* perf measurement windows size 2^x */
#define PERF_CNT_CHECK_WINDOW_SIZE 10
#define task_perf_avg_info(pcd, task_p, class)					\
	tr_info(task_p, "perf_cycle task %p, %pU cpu avg %u peak %u",\
		  class, (class)->uid, \
		  (uint32_t)((pcd)->cpu_delta_sum),			\
		  (uint32_t)((pcd)->cpu_delta_peak))
#define task_perf_cnt_avg(pcd, trace_m, arg, class) do {                             \
		(pcd)->cpu_delta_sum += (pcd)->cpu_delta_last;          \
		if (++(pcd)->sample_cnt == 1 << PERF_CNT_CHECK_WINDOW_SIZE) { \
			(pcd)->cpu_delta_sum >>= PERF_CNT_CHECK_WINDOW_SIZE;      \
			trace_m(pcd, arg, class);                                 \
			(pcd)->cpu_delta_sum = 0;                                 \
			(pcd)->sample_cnt = 0;                                    \
			(pcd)->plat_delta_peak = 0;                               \
			(pcd)->cpu_delta_peak = 0;                                \
		}                                                             \
		} while (0)

/** \brief Accumulates cpu timer delta samples calculated by perf_cnt_stamp().
 *
 *  If current sample count reaches the window size, compute the average and run trace_m.
 *  \param pcd Performance counters data.
 *  \param trace_m Trace function trace_m(pcd, arg) or trace macro if a
 *         more precise line number is desired in the logs.
 *  \param arg Argument passed to trace_m as arg.
 */
#define perf_cnt_average(pcd, trace_m, arg) do {                             \
		(pcd)->cpu_delta_sum += (pcd)->cpu_delta_last;               \
		if (++(pcd)->sample_cnt == 1 << PERF_CNT_CHECK_WINDOW_SIZE) {\
			(pcd)->cpu_delta_sum >>= PERF_CNT_CHECK_WINDOW_SIZE; \
			trace_m(pcd, arg);                                   \
			(pcd)->cpu_delta_sum = 0;                            \
			(pcd)->sample_cnt = 0;                               \
			(pcd)->plat_delta_peak = 0;                          \
			(pcd)->cpu_delta_peak = 0;                           \
		}                                                            \
	} while (0)

/** \brief Reads the timers and computes delta to the previous readings.
 *
 *  If current arch delta exceeds the previous peak value, trace_m is run.
 *  \param pcd Performance counters data.
 *  \param trace_m Trace function trace_m(pcd, arg) or trace macro if a
 *         more precise line number is desired in the logs.
 *  \param arg Argument passed to trace_m as arg.
 */
#define perf_cnt_stamp(pcd, trace_m, arg) do {					\
		uint32_t plat_ts =						\
			(uint32_t)sof_cycle_get_64();				\
		uint32_t cpu_ts =						\
			(uint32_t)perf_cnt_get_cpu_ts();			\
		(pcd)->plat_delta_last = plat_ts - (pcd)->plat_ts;		\
		(pcd)->cpu_delta_last = cpu_ts - (pcd)->cpu_ts;			\
		if ((pcd)->plat_delta_last > (pcd)->plat_delta_peak)		\
			(pcd)->plat_delta_peak = (pcd)->plat_delta_last;	\
		if ((pcd)->cpu_delta_last > (pcd)->cpu_delta_peak) {		\
			(pcd)->cpu_delta_peak = (pcd)->cpu_delta_last;		\
			trace_m(pcd, arg);					\
		}								\
	} while (0)

/**
 * For simple performance measurement and optimization in development stage,
 * tic-toc api is provided. Performance data are traced at each tok call,
 * to allow fast clocks usage deviation estimation. Example:
 *
 * \code{.c}
 * void foo(struct comp_dev *dev) {
 *	static struct perf_cnt_data pcd;
 *
 *	perf_tic(&pcd);
 *	bar();
 *	perf_toc(&pcd, dev);
 * }
 * \endcode
 */

/** \brief Save start timestamp in pcd structure
 *
 * \param pcd Performance counters data.
 */
#define perf_tic(pcd) \
	perf_cnt_init(pcd)

/** \brief Save start timestamp in pcd structure
 *
 * \param pcd Performance counters data.
 * \param comp Component used to get corresponding trace context.
 */
#define perf_toc(pcd, comp) do { \
	perf_cnt_stamp(pcd, perf_trace_null, NULL); \
	perf_trace_simple(pcd, trace_comp_get_tr_ctx(comp)); \
	} while (0)

#else
#define perf_cnt_clear(pcd)
#define perf_cnt_init(pcd)
#define perf_cnt_stamp(pcd, trace_m, arg)
#define perf_cnt_average(pcd, trace_m, arg)
#endif

#endif /* __SOF_LIB_PERF_CNT_H__ */

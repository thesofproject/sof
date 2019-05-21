/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#ifndef __PERFCOUNT_H__
#define __PERFCOUNT_H__

#include <stdint.h>
#include <sof/alloc.h>
#include <sof/clk.h>
#include <sof/platform.h>
#include <sof/list.h>
#include <sof/drivers/timer.h>

#define PERFCOUNT_TRACE_FMT "avg perf: %u us, %u cycles"

#define PERFCOUNT_NUM_STEPS (1 << CONFIG_PERFCOUNT_HISTORY_LOG2)
#define PERFCOUNT_AVG_SHIFT CONFIG_PERFCOUNT_HISTORY_LOG2

/* TODO: Enable traces after runtime trace configuration changes */
#if 0
#define _perfcount_trace(context, prefix) \
	do { \
		typeof(context) _ctx = (context); \
		uint64_t time = platform_timer_get(platform_timer); \
		\
		if (time - _ctx->last_report >= \
		    clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, \
				      CONFIG_PERFCOUNT_INTERVAL)) { \
			uint32_t perf_us = perfcount_get_microseconds( \
				_ctx, PERFCOUNT_STAT_AVG); \
			\
			uint32_t perf_ccount = perfcount_get_ccount( \
				_ctx, PERFCOUNT_STAT_AVG); \
			\
			_ctx->last_report = time; \
			trace_event_with_ids(TRACE_CLASS_PERFCOUNT, \
					     _ctx->measure.task_id, -1, prefix \
					     PERFCOUNT_TRACE_FMT, \
					     perf_us, perf_ccount); \
		} \
	} while (0)
#else
#define _perfcount_trace(context, prefix)
#endif

/**
 * Statistics types
 */
enum perfcount_stats {
	PERFCOUNT_STAT_CUR = 0,
	PERFCOUNT_STAT_AVG,
	PERFCOUNT_STAT_MAX,

	PERFCOUNT_NUM_STATS
};

/**
 * Performance counter sample
 */
struct perfcount_sample {
	uint32_t time_delta;
	uint32_t ccount_delta;
};

/**
 * Measurement results and information
 */
struct perfcount_measure {
	uint32_t task_id;
	struct perfcount_sample stats[PERFCOUNT_NUM_STATS];
};

/**
 * Performance counter context
 */
struct perfcount_context {
	void *mailbox_slot;
	uint32_t cur_step;
	uint64_t begin_time;
	uint64_t begin_ccount;
	uint64_t last_report;
	struct perfcount_sample samples[PERFCOUNT_NUM_STEPS];
	struct perfcount_measure measure;
	struct list_item reuse_list;
};

#if CONFIG_PERFCOUNT

/**
 * \brief Initializes performance counter and returns context
 *
 * The context is used to store current statistics and measurement status,
 * therefore it should be kept and reused with the following calls.
 *
 * \param[in] id Task ID of the measurement
 * \return Measurement context
 */
struct perfcount_context *perfcount_init(uint32_t task_id);

/**
 * \brief Invalidates and frees context
 * \param[in,out] context Measurement context
 */
void perfcount_free(struct perfcount_context **context);

/**
 * \brief Sets the measurement reference
 *
 * Timing mesurements will be taken in relation to the point of calling
 * this function.
 *
 * \param[in] context Measurement context
 */
void perfcount_begin(struct perfcount_context *context);

/**
 * \brief Takes the measurement
 *
 * Performs timing measurements in relation to previously set reference
 * (see perfcount_begin()) and recalculates statistics.
 *
 * \param[in] context Measurement context
 */
void perfcount_end(struct perfcount_context *context);

/**
 * \brief Returns microseconds of a given statistic type
 * \param[in] context Measurement context
 * \param[in] stat_type Statistic type (min, avg, max)
 * \return Time in microseconds
 */
uint64_t perfcount_get_microseconds(struct perfcount_context *context,
				    int stat_type);

/**
 * \brief Returns ccount of a given statistic type
 * \param[in] context Measurement context
 * \param[in] stat_type Statistic type (min, avg, max)
 * \return Number of cycles
 */
uint64_t perfcount_get_ccount(struct perfcount_context *context, int stat_type);

/**
 * \brief Reports the measurement
 *
 * Will report the measurements every interval milliseconds
 * in the form of a trace containing average timing:
 *
 * "<prefix> id: %X avg perf: %u us, %u cycles"
 *
 * \param[in] context Measurement context
 * \param[in] str_prefix String prefix to use in the reported trace
 */
#define perfcount_trace_prefix(context, str_prefix) \
	_perfcount_trace(context, str_prefix " ")

/**
 * \brief Reports the measurement
 *
 * Will report the measurements every interval milliseconds
 * in the form of a trace containing average timing:
 *
 * "id: %X avg perf: %u us, %u cycles"
 *
 * \param[in] context Measurement context
 */
#define perfcount_trace(context) \
	_perfcount_trace(context, "")

#else

static inline struct perfcount_context *perfcount_init(uint32_t task_id)
{
	return NULL;
}

static inline void perfcount_free(struct perfcount_context **context) {}
static inline void perfcount_begin(struct perfcount_context *context) {}
static inline void perfcount_end(struct perfcount_context *context) {}
static inline uint64_t perfcount_get_microseconds(
	struct perfcount_context *context, int stat_type)
{
	return 0;
}

static inline uint64_t perfcount_get_ccount(struct perfcount_context *context,
					    int stat_type)
{
	return 0;
}

#define perfcount_trace_prefix(context, str_prefix)
#define perfcount_trace(context)

#endif

#endif /* __PERFCOUNT_H__ */

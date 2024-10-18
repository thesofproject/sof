/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 *         Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#ifndef __SOF_TRACE_TRACE_H__
#define __SOF_TRACE_TRACE_H__

#ifndef RELATIVE_FILE
#error "This file requires RELATIVE_FILE to be defined. " \
	"Add it to CMake's target with sof_append_relative_path_definitions."
#endif

#include <rtos/sof.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#endif

#if !CONFIG_LIBRARY
#include <platform/trace/trace.h>
#endif
#include <sof/common.h>

#include <sof/trace/preproc.h>
#include <sof/trace/trace-boot.h>

/* Silences compiler warnings about unused variables */
#define trace_unused(class, ctx, id_1, id_2, format, ...) \
	SOF_TRACE_UNUSED(ctx, id_1, id_2, ##__VA_ARGS__)

#if defined(__ZEPHYR__) && defined(CONFIG_ZEPHYR_LOG)

#define tr_err(ctx, fmt, ...) LOG_ERR(fmt, ##__VA_ARGS__)
#define tr_warn(ctx, fmt, ...) LOG_WRN(fmt, ##__VA_ARGS__)
#define tr_info(ctx, fmt, ...) LOG_INF(fmt, ##__VA_ARGS__)
#define tr_dbg(ctx, fmt, ...) LOG_DBG(fmt, ##__VA_ARGS__)

#elif CONFIG_TRACE

#include "trace-soflogger.h"

#else

#define tr_err(ctx, fmt, ...) \
	trace_unused(_TRACE_INV_CLASS, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_warn(ctx, fmt, ...) \
	trace_unused(_TRACE_INV_CLASS, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_info(ctx, fmt, ...) \
	trace_unused(_TRACE_INV_CLASS, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_dbg(ctx, fmt, ...) \
	trace_unused(_TRACE_INV_CLASS, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#endif

#ifndef CONFIG_TRACE

struct trace_filter;
struct sof;

#define trace_event_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)
#define trace_event_atomic_with_ids(class, ctx, id_1, id_2, format, ...) \
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#define trace_warn_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)
#define trace_warn_atomic_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#define tracev_event_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#define trace_point(x)  do {} while (0)

static inline void trace_flush_dma_to_mbox(void) { }
static inline void trace_on(void) { }
static inline void trace_off(void) { }
static inline void trace_init(struct sof *sof) { }
static inline int trace_filter_update(const struct trace_filter *filter)
	{ return 0; }

#define trace_error_with_ids(class, ctx, id_1, id_2, format, ...)      \
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)
#define trace_error_atomic_with_ids(class, ctx, id_1, id_2, format, ...) \
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#define trace_dev_err(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_event_with_ids(_TRACE_INV_CLASS, get_ctx_m(dev),		\
			     get_id_m(dev), get_subid_m(dev),		\
			     fmt, ##__VA_ARGS__)
#define trace_dev_warn(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_event_with_ids(_TRACE_INV_CLASS, get_ctx_m(dev),		\
			     get_id_m(dev), get_subid_m(dev),		\
			     fmt, ##__VA_ARGS__)
#define trace_dev_info(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_event_with_ids(_TRACE_INV_CLASS, get_ctx_m(dev),		\
			     get_id_m(dev), get_subid_m(dev),		\
			     fmt, ##__VA_ARGS__)
#define trace_dev_dbg(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_event_with_ids(_TRACE_INV_CLASS, get_ctx_m(dev),		\
			     get_id_m(dev), get_subid_m(dev),		\
			     fmt, ##__VA_ARGS__)

static inline void mtrace_printf(int log_level, const char *format_str, ...)
{
};

#endif /* !CONFIG_TRACE */

/** Default value when there is no specific pipeline, dev, dai, etc. */
#define _TRACE_INV_ID		-1

/** This has been replaced in commits 6ce635aa82 and earlier by the
 * DECLARE_TR_CTX, tr_ctx and component UUID system below
 */
#define _TRACE_INV_CLASS	TRACE_CLASS_DEPRECATED

/**
 * Trace context.
 */
struct tr_ctx {
	const struct sof_uuid_entry *uuid_p;	/**< UUID pointer, use SOF_UUID() to init */
	uint32_t level;				/**< Default log level */
};

#if defined(UNIT_TEST) || !defined(CONFIG_TRACE)
#define TRACE_CONTEXT_SECTION
#else
#define TRACE_CONTEXT_SECTION __section(".trace_ctx")
#endif

/**
 * Declares trace context.
 * @param ctx_name (Symbol) name.
 * @param uuid UUID pointer, use SOF_UUID() to inititalize.
 * @param default_log_level Default log level.
 */
#define DECLARE_TR_CTX(ctx_name, uuid, default_log_level)	\
	struct tr_ctx ctx_name TRACE_CONTEXT_SECTION = {	\
		.uuid_p = uuid,					\
		.level = default_log_level,			\
	}

/* Only define these two macros for XTOS to avoid the collision with
 * zephyr/include/zephyr/logging/log.h
 */
#ifndef __ZEPHYR__
#define LOG_MODULE_REGISTER(ctx, level)
#define LOG_MODULE_DECLARE(ctx, level)
#endif

#endif /* __SOF_TRACE_TRACE_H__ */

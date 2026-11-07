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
#endif
#include <zephyr/logging/log.h>

#include <sof/common.h>

#if CONFIG_ZEPHYR_LOG || CONFIG_LIBRARY || CONFIG_ARCH_POSIX_LIBFUZZER
#include <sof/lib/uuid.h>
#endif
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

#elif CONFIG_LIBRARY && !defined(__ZEPHYR__)

#include "trace-host.h"

#else

#define tr_err(ctx, fmt, ...) \
	trace_unused(_TRACE_INV_CLASS, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_warn(ctx, fmt, ...) \
	trace_unused(_TRACE_INV_CLASS, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_info(ctx, fmt, ...) \
	trace_unused(_TRACE_INV_CLASS, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_dbg(ctx, fmt, ...) \
	trace_unused(_TRACE_INV_CLASS, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define trace_dev_err(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_unused(_TRACE_INV_CLASS, get_ctx_m(dev), get_id_m(dev),	\
		     get_subid_m(dev), fmt, ##__VA_ARGS__)
#define trace_dev_warn(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_unused(_TRACE_INV_CLASS, get_ctx_m(dev), get_id_m(dev),	\
		     get_subid_m(dev), fmt, ##__VA_ARGS__)
#define trace_dev_info(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_unused(_TRACE_INV_CLASS, get_ctx_m(dev), get_id_m(dev),	\
		     get_subid_m(dev), fmt, ##__VA_ARGS__)
#define trace_dev_dbg(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_unused(_TRACE_INV_CLASS, get_ctx_m(dev), get_id_m(dev),	\
		     get_subid_m(dev), fmt, ##__VA_ARGS__)

#endif

/* Boot progress markers; no longer reported anywhere */
#define trace_point(x)  do {} while (0)

/** Default value when there is no specific pipeline, dev, dai, etc. */
#define _TRACE_INV_ID		-1

/** This has been replaced in commits 6ce635aa82 and earlier by the
 * DECLARE_TR_CTX, tr_ctx and component UUID system below
 */
#define _TRACE_INV_CLASS	TRACE_CLASS_DEPRECATED

/**
 * Trace context.
 */
struct sof_uuid_entry;
struct tr_ctx {
	const struct sof_uuid_entry *uuid_p;	/**< UUID pointer, use SOF_UUID() to init */
	uint32_t level;				/**< Default log level */
};

/**
 * Declares trace context.
 * @param ctx_name (Symbol) name.
 * @param uuid UUID pointer, use SOF_UUID() to inititalize.
 * @param default_log_level Default log level.
 */
#define DECLARE_TR_CTX(ctx_name, uuid, default_log_level)	\
	struct tr_ctx ctx_name = {			\
		.uuid_p = uuid,					\
		.level = default_log_level,			\
	}

#endif /* __SOF_TRACE_TRACE_H__ */

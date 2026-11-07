/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016,2024 Intel Corporation.
 */

/**
 * \file include/sof/trace/trace-host.h
 * \brief Trace output of the host builds (testbench, host library).
 * \ingroup trace_logging_api
 *
 * Messages are printed to stderr when their level is at or below
 * host_trace_level, which the testbench sets with its -d option.
 */

#ifndef __SOF_TRACE_TRACE_HOST_H__
#define __SOF_TRACE_TRACE_HOST_H__

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <user/trace.h> /* LOG_LEVEL_... */

/* trace level used on host configurations */
extern int host_trace_level;

char *get_trace_class(uint32_t trace_class);

#define _log_message(level, ctx, id_1, id_2, format, ...)		\
do {									\
	(void)ctx;							\
	(void)id_1;							\
	(void)id_2;							\
	struct timeval tv;						\
	char slevel[5][4] = {"dis", "err", "wrn", "inf", "dbg"};	\
	char *msg = "(%s:%d) " format;					\
	if (level <= host_trace_level) {				\
		gettimeofday(&tv, NULL);				\
		fprintf(stderr, "trace: [%ld.%6.6ld] <%s> ", tv.tv_sec, tv.tv_usec, \
			slevel[level]);					\
		fprintf(stderr, msg, strrchr(__FILE__, '/') + 1,	\
			__LINE__, ##__VA_ARGS__);			\
		fprintf(stderr, "\n");					\
	}								\
} while (0)

/* tracing from device (component, pipeline, dai, ...) */

#define trace_dev_err(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	_log_message(LOG_LEVEL_CRITICAL, get_ctx_m(dev), get_id_m(dev),	\
		     get_subid_m(dev), fmt, ##__VA_ARGS__)

#define trace_dev_warn(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	_log_message(LOG_LEVEL_WARNING, get_ctx_m(dev), get_id_m(dev),	\
		     get_subid_m(dev), fmt, ##__VA_ARGS__)

#define trace_dev_info(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	_log_message(LOG_LEVEL_INFO, get_ctx_m(dev), get_id_m(dev),	\
		     get_subid_m(dev), fmt, ##__VA_ARGS__)

#define trace_dev_dbg(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	_log_message(LOG_LEVEL_VERBOSE, get_ctx_m(dev), get_id_m(dev),	\
		     get_subid_m(dev), fmt, ##__VA_ARGS__)

/* tracing from infrastructure part */

#define tr_err(ctx, fmt, ...) \
	_log_message(LOG_LEVEL_CRITICAL, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_warn(ctx, fmt, ...) \
	_log_message(LOG_LEVEL_WARNING, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_info(ctx, fmt, ...) \
	_log_message(LOG_LEVEL_INFO, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)
#define tr_dbg(ctx, fmt, ...) \
	_log_message(LOG_LEVEL_VERBOSE, ctx, _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#endif /* __SOF_TRACE_TRACE_HOST_H__ */

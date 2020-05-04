/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __INCLUDE_TRACE__
#define __INCLUDE_TRACE__

#ifndef ZEPHYR_INCLUDE_LOGGING_LOG_H_
#include <logging/log.h>
LOG_MODULE_DECLARE(sof, LOG_LEVEL_DBG);
#endif

#include <sof-config.h>

struct timer;
uint64_t platform_timer_get(struct timer *);

#include <user/trace.h>

#define trace_point(x)
#define trace_event(class, format, ...)		\
	_log_message(LOG_LEVEL_INF, class, format, ##__VA_ARGS__)
#define trace_warn(class, format, ...)		\
	_log_message(LOG_LEVEL_WRN, class, format,  ##__VA_ARGS__)
#define trace_error(class, format, ...)		\
	_log_message(LOG_LEVEL_ERR, class, format,  ##__VA_ARGS__)

#define trace_event_with_ids(class, id_0, id_1, id_2, format, ...)	\
	_log_message(LOG_LEVEL_INF, class, format, ##__VA_ARGS__)
#define trace_error_with_ids(class, id_0, id_1, id_2, format, ...)	\
	_log_message(LOG_LEVEL_ERR, class, format, ##__VA_ARGS__)
#define trace_warn_with_ids(class, id_0, id_1, id_2, format, ...)	\
	_log_message(LOG_LEVEL_WRN, class, format,  ##__VA_ARGS__)

/* TODO: Verbose handling */
#if CONFIG_TRACE_VERBOSE
#define tracev_event(...)		trace_event(__VA_ARGS__)
#define tracev_event_with_ids(...)	trace_event_with_ids(__VA_ARGS__)
#else
#define tracev_event(...)
#define tracev_event_with_ids(...)
#endif
#define tracev_event_atomic(...)
#define tracev_event_atomic_with_ids(...)
#define tracev_value(x)
#define tracev_value_atomic(x)

#define trace_on(...)
#define trace_off(...)

#define trace_error_atomic(...)
#define trace_event_atomic(...)

#define trace_dev_dbg(class, get_uid_m, get_id_m, get_subid_m, dev, fmt, ...) \
	tracev_event_with_ids(class, get_uid_m(dev), get_id_m(dev),	      \
			      get_subid_m(dev), fmt, ##__VA_ARGS__)
#define trace_dev_info(class, get_uid_m, get_id_m, get_subid_m, dev, fmt, ...) \
	trace_event_with_ids(class, get_uid_m(dev), get_id_m(dev),	       \
			     get_subid_m(dev), fmt, ##__VA_ARGS__)
#define trace_dev_err(class, get_uid_m, get_id_m, get_subid_m, dev, fmt, ...) \
	trace_error_with_ids(class, get_uid_m(dev), get_id_m(dev),	      \
			     get_subid_m(dev), fmt, ##__VA_ARGS__)
#define trace_dev_warn(class, get_uid_m, get_id_m, get_subid_m, dev, fmt, ...) \
	trace_warn_with_ids(class, get_uid_m(dev), get_id_m(dev),	       \
			    get_subid_m(dev), fmt, ##__VA_ARGS__)


static inline void trace_flush(void) { }
static inline void trace_log(void) { }

char *get_trace_class(uint32_t trace_class);

#define _log_message(level, class, format, ...)				\
	do {								\
		Z_LOG(level, "%d:%s " format, (uint32_t)(platform_timer_get(NULL) & 0xffffffff), get_trace_class(class),	\
		      ##__VA_ARGS__);					\
	} while (0)

#endif /* __INCLUDE_TRACE__ */

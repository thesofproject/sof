/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __INCLUDE_TRACE__
#define __INCLUDE_TRACE__

/**
 * Placeholder for trace with Zephyr logging subsystem
 */

#define trace_point(x)
#define trace_event(...)

#define tracev_event(...)
#define tracev_event_with_ids(...)
#define tracev_event_atomic(...)
#define tracev_event_atomic_with_ids(...)
#define tracev_value(x)
#define tracev_value_atomic(x)

#define trace_error(...)
#define trace_warn(...)

#define trace_on(...)
#define trace_off(...)

#define trace_error_atomic(...)
#define trace_event_atomic(...)

#define trace_error_with_ids(...)
#define trace_event_with_ids(...)

#define trace_dev_dbg(...)
#define trace_dev_info(...)
#define trace_dev_err(...)
#define trace_dev_warn(...)

static inline void trace_flush(void) { }
static inline void trace_log(void) { }

#endif /* __INCLUDE_TRACE__ */

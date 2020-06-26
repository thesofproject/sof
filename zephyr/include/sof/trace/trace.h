/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SOF_TRACE_TRACE1_H__
#define __SOF_TRACE_TRACE1_H__

#ifndef ZEPHYR_INCLUDE_LOGGING_LOG_H_
#include <logging/log.h>
LOG_MODULE_DECLARE(sof, LOG_LEVEL_DBG);
#endif

/* SOF trace header */
#include "../../../../src/include/sof/trace/trace.h"

struct timer;
uint64_t platform_timer_get(struct timer *);

/*
 * Use SOF macros, but let Zephyr take care of the physical log IO.
 */
#undef _log_message
#define _log_message(atomic, level, comp_class, ctx, id1, id2, format, ...)				\
	do {								\
		Z_LOG(level, "%d: " format, (uint32_t)(platform_timer_get(NULL) & 0xffffffff),	\
		      ##__VA_ARGS__);					\
	} while (0)

#endif /* __INCLUDE_TRACE__ */

// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//

#ifndef __SOF_TRACE_TRACE1_H__
#define __SOF_TRACE_TRACE1_H__

#ifndef ZEPHYR_INCLUDE_LOGGING_LOG_H_
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

/* Level of SOF trace on Zephyr */
#define SOF_ZEPHYR_TRACE_LEVEL LOG_LEVEL_INF

#endif

/* printk supports uint64_t so use it until LOG is ready */
#define USE_PRINTK	1

/* SOF trace header */
#include "../../../../src/include/sof/trace/trace.h"

struct timer;
uint64_t platform_timer_get(struct timer *timer);

/*
 * Override SOF mtrace_printf() macro for now to support Zephyr's shared
 * memory logger. Note the DMA trace can be copied to the shared memory
 * too thanks to CONFIG_TRACEM.
 */
#undef mtrace_printf

#if USE_PRINTK
#define mtrace_printf(level, format, ...)					\
	do {									\
		if ((level) <= SOF_ZEPHYR_TRACE_LEVEL)				\
			printk("%llu: " format "\n", k_cycle_get_64(),		\
				##__VA_ARGS__);					\
	} while (0)
#else
#error "TODO: Z_LOG()"
#endif

#endif /* __INCLUDE_TRACE__ */

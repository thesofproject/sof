/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_BUF_COPIER_DMA_H__
#define __SOF_DRIVERS_BUF_COPIER_DMA_H__

#include <sof/lib/dma.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#define trace_buf_copier(__e, ...) \
	trace_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define tracev_buf_copier(__e, ...) \
	tracev_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define trace_buf_copier_error(__e, ...) \
	trace_error(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)

extern const struct dma_ops buf_copier_dma_ops;

#endif /* __SOF_DRIVERS_BUF_COPIER_DMA_H__ */

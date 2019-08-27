// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <sof/probe/probe.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#define trace_probe(__e, ...) \
	trace_event(TRACE_CLASS_PROBE, __e, ##__VA_ARGS__)
#define tracev_probe(__e, ...) \
	tracev_event(TRACE_CLASS_PROBE, __e, ##__VA_ARGS__)
#define trace_probe_error(__e, ...) \
	trace_error(TRACE_CLASS_PROBE, __e, ##__VA_ARGS__)

int probe_init(struct probe_dma *probe_dma)
{
	return 0;
}

int probe_deinit(void)
{
	return 0;
}

int probe_dma_add(uint32_t count, struct probe_dma *probe_dma)
{
	return 0;
}

int probe_dma_info(struct sof_ipc_probe_info_params *data, uint32_t max_size)
{
	return 0;
}

int probe_dma_remove(uint32_t count, uint32_t *stream_tag)
{
	return 0;
}

int probe_point_add(uint32_t count, struct probe_point *probe)
{
	return 0;
}

int probe_point_info(struct sof_ipc_probe_info_params *data, uint32_t max_size)
{
	return 0;
}

int probe_point_remove(uint32_t count, uint32_t *buffer_id)
{
	return 0;
}

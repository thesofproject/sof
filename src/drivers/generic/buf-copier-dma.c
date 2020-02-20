// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
//

 /**
  * \file drivers/generic/buf-copier-dma.c
  * \brief Driver for Virtual DMA with buffer copying capability.
  * \author Artur Kloniecki <arturx.kloniecki@linux.intel.com>
  */

#include <sof/drivers/buf-copier-dma.h>
#include <sof/lib/dma.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

static struct dma_chan_data *buf_copier_dma_channel_get(struct dma *dma,
							unsigned int req_chan)
{
	trace_buf_copier("buf_copier_dma_channel_get()");
	return NULL;
}

static void buf_copier_dma_channel_put(struct dma_chan_data *channel)
{
	trace_buf_copier("buf_copier_dma_channel_put()");
}

static int buf_copier_dma_start(struct dma_chan_data *channel)
{
	trace_buf_copier("buf_copier_dma_start()");
	return 0;
}

static int buf_copier_dma_stop(struct dma_chan_data *channel)
{
	trace_buf_copier("buf_copier_dma_stop()");
	return 0;
}

static int buf_copier_dma_copy(struct dma_chan_data *channel, int bytes,
			       uint32_t flags)
{
	trace_buf_copier("buf_copier_dma_copy()");
	return 0;
}

static int buf_copier_dma_pause(struct dma_chan_data *channel)
{
	trace_buf_copier("buf_copier_dma_pause()");
	return 0;
}

static int buf_copier_dma_release(struct dma_chan_data *channel)
{
	trace_buf_copier("buf_copier_dma_release()");
	return 0;
}

static int buf_copier_dma_status(struct dma_chan_data *channel,
				 struct dma_chan_status *status,
				 uint8_t direction)
{
	trace_buf_copier("buf_copier_dma_status()");
	return 0;
}

static int buf_copier_dma_set_config(struct dma_chan_data *channel,
				     struct dma_sg_config *config)
{
	trace_buf_copier("buf_copier_dma_set_config()");
	return 0;
}

static int buf_copier_dma_dummy(struct dma *dma)
{
	trace_buf_copier("buf_copier_dma_dummy()");
	return 0;
}

static int buf_copier_dma_probe(struct dma *dma)
{
	trace_buf_copier("buf_copier_dma_probe()");
	return 0;
}

static int buf_copier_dma_remove(struct dma *dma)
{
	trace_buf_copier("buf_copier_dma_remove()");
	return 0;
}

static int buf_copier_dma_get_data_size(struct dma_chan_data *channel,
					uint32_t *avail, uint32_t *free)
{
	trace_buf_copier("buf_copier_dma_get_data_size()");
	return 0;
}

static int buf_copier_dma_get_attribute(struct dma *dma, uint32_t type,
					uint32_t *value)
{
	trace_buf_copier("buf_copier_dma_get_attribute()");
	return 0;
}

static int buf_copier_dma_interrupt(struct dma_chan_data *channel,
				    enum dma_irq_cmd cmd)
{
	trace_buf_copier("buf_copier_dma_interrupt()");
	return 0;
}

const struct dma_ops buf_copier_dma_ops = {
	.channel_get = buf_copier_dma_channel_get,
	.channel_put = buf_copier_dma_channel_put,
	.start = buf_copier_dma_start,
	.stop = buf_copier_dma_stop,
	.copy = buf_copier_dma_copy,
	.pause = buf_copier_dma_pause,
	.release = buf_copier_dma_release,
	.status = buf_copier_dma_status,
	.set_config = buf_copier_dma_set_config,
	.pm_context_restore = buf_copier_dma_dummy,
	.pm_context_store = buf_copier_dma_dummy,
	.probe = buf_copier_dma_probe,
	.remove = buf_copier_dma_remove,
	.get_data_size = buf_copier_dma_get_data_size,
	.get_attribute = buf_copier_dma_get_attribute,
	.interrupt = buf_copier_dma_interrupt,
};

/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Keyon Jie <yang.jie@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/trace.h>
#include <reef/dma.h>
#include <reef/ipc.h>
#include <reef/wait.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <platform/dma.h>
#include <arch/cache.h>
#include <uapi/ipc.h>

#define trace_host(__e)	trace_event(TRACE_CLASS_HOST, __e)
#define tracev_host(__e)	tracev_event(TRACE_CLASS_HOST, __e)
#define trace_host_error(__e)	trace_error(TRACE_CLASS_HOST, __e)

struct host_gw_data {
	/* local DMA config */
	struct dma *dma;
	struct host_dma_config config;
	struct comp_buffer *dma_buffer;

	/* local and host DMA buffer info */
	uint32_t host_size;
	/* host possition reporting related */
	volatile uint32_t *host_pos;    /* read/write pos, update to mailbox for host side */
	uint32_t report_pos;		/* position in current report period */
	uint32_t local_pos;		/* the host side buffer local read/write possition, in bytes */
	/* pointers set during params to host or local above */
	struct hc_buf *source;
	struct hc_buf *sink;
	uint32_t period_bytes;
	uint32_t period_count;
	uint32_t first_copy;
	uint32_t thd_size;

	/* stream info */
	struct sof_ipc_stream_posn posn; /* TODO: update this */
};

static struct comp_dev *host_gw_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct host_gw_data *hd;
	struct sof_ipc_comp_host *host;
	struct sof_ipc_comp_host *ipc_host = (struct sof_ipc_comp_host *)comp;

	trace_host("new");

	dev = rzalloc(RZONE_RUNTIME, RFLAGS_NONE,
		COMP_SIZE(struct sof_ipc_comp_host));
	if (dev == NULL)
		return NULL;

	host = (struct sof_ipc_comp_host *)&dev->comp;
	memcpy(host, ipc_host, sizeof(struct sof_ipc_comp_host));

	hd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*hd));
	if (hd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, hd);

	if (ipc_host->direction == SOF_IPC_STREAM_PLAYBACK)
		hd->dma = dma_get(DMA_HOST_OUT_DMAC);
	else
		hd->dma = dma_get(DMA_HOST_IN_DMAC);

	dev->state = COMP_STATE_READY;

	return dev;

}

static void host_gw_free(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	trace_host("fre");

	rfree(hd);
	rfree(dev);
}

/* configure the DMA params and descriptors for host buffer IO */
static int host_gw_params(struct comp_dev *dev)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *sconfig;
	uint32_t buffer_size ;

	trace_host("par");

	/* host params always installed by pipeline IPC */
	hd->host_size = dev->params.buffer.size;

	/* determine source and sink buffer elems */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {

		hd->dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);
		sconfig = COMP_GET_CONFIG(hd->dma_buffer->sink);

		hd->period_count = sconfig->periods_source;
	} else {

		hd->dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);
		sconfig = COMP_GET_CONFIG(hd->dma_buffer->source);

		hd->period_count = sconfig->periods_sink;
	}

	/* calculate period size based on config */
	hd->period_bytes = dev->frames * comp_frame_bytes(dev);
	if (hd->period_bytes == 0) {
		trace_host_error("eS1");
		return -EINVAL;
	}

	dev->frame_bytes = comp_frame_bytes(dev);

	/* resize the buffer if space is available to align with period size */
	buffer_size = hd->period_count * hd->period_bytes;
	if (buffer_size <= hd->dma_buffer->alloc_size)
		hd->dma_buffer->size = buffer_size;
	else {
		trace_host_error("eSz");
		return -EINVAL;
	}

	/* component buffer size must be divisor of host buffer size */
	if (hd->host_size % hd->period_bytes) {
		trace_comp_error("eHB");
		trace_value(hd->host_size);
		trace_value(hd->period_bytes);
		return -EINVAL;
	}

	hd->dma_buffer->r_ptr = hd->dma_buffer->addr;
	hd->dma_buffer->w_ptr = hd->dma_buffer->addr;

	if(dev->params.frame_fmt == SOF_IPC_FRAME_S16_LE)
		hd->config.cs = 0x80800000;//SCS=0 for 32bit container; FIFORDY=0
	else
		hd->config.cs = 0x00800000;
	hd->config.bba = (uint32_t)hd->dma_buffer->addr;
	hd->config.bs = hd->dma_buffer->size;
	hd->config.bfpi = 0;
	hd->config.bsp = hd->period_bytes;
	hd->config.mbs = hd->period_bytes;//hd->dma_buffer->size;
	hd->config.llpi = 0;
	hd->config.lpibi = 0;

	dev->params.stream_tag -=1;

	return 0;
}

static int host_gw_config(struct comp_dev *dev)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);
	struct host_dma_config *host_config = &hd->config;
	uint32_t is_out, stream_id;

	trace_host("GwC");

	is_out = (dev->params.direction == SOF_IPC_STREAM_PLAYBACK);
	stream_id = dev->params.stream_tag;

	host_dma_reg_write(is_out, stream_id, DGBBA, host_config->bba);
	host_dma_reg_write(is_out, stream_id, DGBS, host_config->bs);
	host_dma_reg_write(is_out, stream_id, DGCS, host_config->cs);
	host_dma_reg_write(is_out, stream_id, DGBFPI, host_config->bfpi);
	host_dma_reg_write(is_out, stream_id, DGBSP, host_config->bsp);
	host_dma_reg_write(is_out, stream_id, DGMBS, host_config->mbs);
	host_dma_reg_write(is_out, stream_id, DGLLPI, host_config->llpi);
	host_dma_reg_write(is_out, stream_id, DGLPIBI, host_config->lpibi);

	trace_value(stream_id);

	trace_host("GcD");

#if 0
	/* dump dma host registers */
	for (j = 0; j < GTW_HOST_OUT_STREAM_SIZE; j += 4)
		trace_value(*(uint32_t *)(GTW_HOST_OUT_STREAM_BASE(dev->params.stream_tag) + j));
#endif
	return 0;
}

static int host_gw_prepare(struct comp_dev *dev)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;
	int ret;

	trace_host("pre");

	ret = comp_set_state(dev, COMP_CMD_PREPARE);
	if (ret < 0)
		return ret;

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK)
		dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);
	else
		dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);
	dma_buffer->r_ptr = dma_buffer->w_ptr = dma_buffer->addr;

	/* initialize buffer as full(all 0s) */
	comp_update_buffer_produce(dma_buffer, dma_buffer->size);

	hd->local_pos = 0;
	if (hd->host_pos)
		*hd->host_pos = 0;
	hd->report_pos = 0;

	hd->first_copy = 1;

	hd->thd_size = hd->period_bytes;

	return 0;
}

static int host_gw_pointer_reset(struct comp_dev *dev)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);

	/* reset buffer pointers */
	if (hd->host_pos)
		*hd->host_pos = 0;
	hd->local_pos = 0;
	hd->report_pos = 0;

	comp_set_state(dev, COMP_CMD_RESET);

	return 0;
}

static int host_gw_start(struct comp_dev *dev)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);
	struct host_dma_config *host_config = &hd->config;
	uint32_t is_out, stream_id;

	trace_host("GwS");

	is_out = (dev->params.direction == SOF_IPC_STREAM_PLAYBACK);
	stream_id = dev->params.stream_tag;

	host_config->cs |= 0x04000020;
	host_dma_reg_write(is_out, stream_id, DGCS, host_config->cs);

	return 0;
}

static int host_gw_stop(struct comp_dev *dev)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);
	struct host_dma_config *host_config = &hd->config;
	uint32_t is_out, stream_id;

	trace_host("GwX");

	is_out = (dev->params.direction == SOF_IPC_STREAM_PLAYBACK);
	stream_id = dev->params.stream_tag;

	host_config->cs &= ~0x04000020;
	host_dma_reg_write(is_out, stream_id, DGCS, host_config->cs);

	/* reset host side buffer pointers */
	host_gw_pointer_reset(dev);

	dev->state = COMP_STATE_PAUSED;

	return 0;
}


static int host_gw_position(struct comp_dev *dev,
	struct sof_ipc_stream_posn *posn)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);

	/* TODO: improve accuracy by adding current DMA position */
	posn->host_posn = hd->local_pos;

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int host_gw_cmd(struct comp_dev *dev, int cmd, void *data)
{
	int ret = 0;

	trace_host("cmd");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	// TODO: align cmd macros.
	switch (cmd) {
	case COMP_CMD_PAUSE:
	case COMP_CMD_STOP:
		ret = host_gw_stop(dev);
		break;
	case COMP_CMD_RELEASE:
		break;
	case COMP_CMD_START:
		trace_host("HSt");

		/* config & start */
		host_gw_config(dev);

		ret = host_gw_start(dev);

		break;
	case COMP_CMD_SUSPEND:
	case COMP_CMD_RESUME:
		break;
	default:
		break;
	}

	return ret;
}

static int host_gw_reset(struct comp_dev *dev)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);

	trace_host("res");

	host_gw_pointer_reset(dev);
	hd->host_pos = NULL;
	hd->source = NULL;
	hd->sink = NULL;

	dev->state = COMP_STATE_READY;

	return 0;
}

/* buffer update callback */
static int host_gw_buffer_update(struct comp_dev *dev,
	struct comp_buffer *buffer, uint32_t size)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);
	struct host_dma_config *host_config = &hd->config;
	struct comp_buffer *dma_buffer = hd->dma_buffer;
	uint32_t is_out, stream_id;

	trace_host("GwU");

	if ((dev->state != COMP_STATE_ACTIVE) || (dma_buffer != buffer))
		return -EINVAL;

	is_out = (dev->params.direction == SOF_IPC_STREAM_PLAYBACK);
	stream_id = dev->params.stream_tag;

	trace_value(host_dma_reg_read(is_out, stream_id, DGBRP));
	trace_value(host_dma_reg_read(is_out, stream_id, DGBWP));

	host_config->bfpi = size;

	/* reset BSC before start next copy */
	host_dma_reg_write(is_out, stream_id, DGCS,
		host_dma_reg_read(is_out, stream_id, DGCS) | DGCS_BSC);

	trace_value(size);
	trace_value(host_dma_reg_read(is_out, stream_id, DGCS));

	/*
	 * set BFPI to let host gateway knows we have read size,
	 * which will trigger next copy start.
	 */
	host_dma_reg_write(is_out, stream_id, DGBFPI, size);

	host_dma_reg_write(is_out, stream_id, DGLLPI, size);
	host_dma_reg_write(is_out, stream_id, DGLPIBI, size);

	trace_value(host_dma_reg_read(is_out, stream_id, DGCS));

	/* new local period, update host buffer position blks */
	hd->local_pos += size;

	/* buffer overlap, hard code host buffer size at the moment ? */
	if (hd->local_pos >= hd->host_size)
		hd->local_pos = 0;

	/* send IPC message to driver if needed */
	hd->report_pos += size;
	/* update for host side */
	if (hd->host_pos)
		*hd->host_pos = hd->local_pos;

	/* NO_IRQ mode if host_period_size == 0 */
	if (dev->params.host_period_bytes != 0 &&
		hd->report_pos >= dev->params.host_period_bytes) {
		hd->report_pos = 0;

		/* send timestamps to host */
		pipeline_get_timestamp(dev->pipeline, dev, &hd->posn);
		ipc_stream_send_position(dev, &hd->posn);
	}

	return 0;
}

/*
 * copy and process stream data from source to sink buffers
 * Todo: fix it for pass through topology: host - B0 - dai
 */
static int host_gw_copy(struct comp_dev *dev)
{
	struct host_gw_data *hd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;
	uint32_t is_out, stream_id;
	uint32_t new_wr_size = 0;
	uint32_t w_ptr;
	uint32_t new_rd_size = 0;
	uint32_t r_ptr;

	trace_host("cpy");

	dma_buffer = hd->dma_buffer;

	is_out = (dev->params.direction == SOF_IPC_STREAM_PLAYBACK);
	stream_id = dev->params.stream_tag;

	if (hd->first_copy) {
		/* for the 1st copy, we need align the HW r/w ptr with SW ones */
		while (!(host_dma_reg_read(is_out, stream_id, DGCS) & DGCS_BF));
		trace_host("CbF");
		trace_value(host_dma_reg_read(is_out, stream_id, DGBRP));
		trace_value(host_dma_reg_read(is_out, stream_id, DGBWP));
		trace_value(host_dma_reg_read(is_out, stream_id, DGCS));

		/* the first period copied, start the 2nd one */
		host_gw_buffer_update(dev, dma_buffer, hd->period_bytes);
		while (!(host_dma_reg_read(is_out, stream_id, DGCS) & DGCS_BF));
		/* the 2nd period copied */
		trace_host("CbF");
		trace_value(host_dma_reg_read(is_out, stream_id, DGBRP));
		trace_value(host_dma_reg_read(is_out, stream_id, DGBWP));
		trace_value(host_dma_reg_read(is_out, stream_id, DGCS));

		/*
		 * here we should be aligned with the initialized SW pointers:
		 * w_ptr = r_ptr = 0, buffer full.
		 */
		hd->first_copy = 0;
	}

	/* check if need to start next copy */
	trace_value(dma_buffer->free);

	if (dma_buffer->free < hd->thd_size)
		return 0;

	r_ptr = host_dma_reg_read(is_out, stream_id, DGBRP);

	new_rd_size = (uint32_t) (dma_buffer->r_ptr - dma_buffer->addr) >= r_ptr ?
		(dma_buffer->r_ptr - dma_buffer->addr) - r_ptr :
		(dma_buffer->r_ptr - dma_buffer->addr) + dma_buffer->size - r_ptr;

	if (new_rd_size && (new_rd_size >= hd->thd_size))
		/* update r_ptr to gateway and start next copy */
		host_gw_buffer_update(dev, dma_buffer, new_rd_size);

	/* check if last copy finished */
	while (!(host_dma_reg_read(is_out, stream_id, DGCS) & DGCS_BF)) {
		/* sleep some time ? */
		trace_value(host_dma_reg_read(is_out, stream_id, DGCS));
	}
	trace_host("CbF");
	trace_value(host_dma_reg_read(is_out, stream_id, DGBRP));
	trace_value(host_dma_reg_read(is_out, stream_id, DGBWP));
	trace_value(host_dma_reg_read(is_out, stream_id, DGCS));

	/* buffer full */
	if (host_dma_reg_read(is_out, stream_id, DGCS) & DGCS_BF) {
		w_ptr = host_dma_reg_read(is_out, stream_id, DGBWP);
		new_wr_size = w_ptr >= (uint32_t) (dma_buffer->w_ptr - dma_buffer->addr) ?
			w_ptr - (dma_buffer->w_ptr - dma_buffer->addr) :
			w_ptr + dma_buffer->size - (dma_buffer->w_ptr - dma_buffer->addr);

		trace_value(w_ptr);
		trace_value(new_wr_size);

		if (new_wr_size)
			/* update dma buffer write pointer */
			comp_update_buffer_produce(dma_buffer, new_wr_size);
	}

	return 0;
}

struct comp_driver comp_gw_host = {
	.type	= SOF_COMP_HOST,
	.ops	= {
		.new		= host_gw_new,
		.free		= host_gw_free,
		.params		= host_gw_params,
		.reset		= host_gw_reset,
		.cmd		= host_gw_cmd,
		.copy		= host_gw_copy,
		.prepare	= host_gw_prepare,
		.position	= host_gw_position,
	},
};

void sys_comp_host_init(void)
{
	comp_register(&comp_gw_host);
}


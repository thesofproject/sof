/*
 * Copyright (c) 2017, Intel Corporation
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
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/work.h>
#include <reef/clock.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <uapi/ipc.h>
#include "src_core.h"

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#define trace_src(__e) trace_event(TRACE_CLASS_SRC, __e)
#define tracev_src(__e) tracev_event(TRACE_CLASS_SRC, __e)
#define trace_src_error(__e) trace_error(TRACE_CLASS_SRC, __e)

/* src component private data */
struct comp_data {
	struct polyphase_src src;
	struct src_param param;
	int32_t *delay_lines;
	uint32_t sink_rate;
	uint32_t source_rate;
	int32_t *sbuf_w_ptr;
	int32_t *sbuf_r_ptr;
	int sbuf_avail;
	void (* src_func)(struct comp_dev *dev,
		struct comp_buffer *source,
		struct comp_buffer *sink,
		size_t *consumed,
		size_t *produced);
	void (* polyphase_func)(struct src_stage_prm *s);
};

/* Fallback function */
static void src_fallback(struct comp_dev *dev, struct comp_buffer *source,
	struct comp_buffer *sink, size_t *bytes_read, size_t *bytes_written)
{
	*bytes_read = 0;
	*bytes_written = 0;
}

/* Normal 2 stage SRC */
static void src_2s_s32_default(struct comp_dev *dev,
	struct comp_buffer *source, struct comp_buffer *sink,
	size_t *bytes_read, size_t *bytes_written)
{
	struct src_stage_prm s1;
	struct src_stage_prm s2;
	int s1_blk_in;
	int s1_blk_out;
	int s2_blk_in;
	int s2_blk_out;
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *dest = (int32_t *) sink->w_ptr;
	int32_t *src = (int32_t *) source->r_ptr;
	int32_t *sbuf_end_addr = &cd->delay_lines[cd->param.sbuf_length];
	int32_t sbuf_size = cd->param.sbuf_length * sizeof(int32_t);
	int nch = dev->params.channels;
	int sbuf_free = cd->param.sbuf_length - cd->sbuf_avail;
	int n_read = 0;
	int n_written = 0;
	int n1 = 0;
	int n2 = 0;
	int avail_b = source->avail;
	int free_b = sink->free;
	int sz = sizeof(int32_t);

	s1.x_end_addr = source->end_addr;
	s1.x_size = source->size;
	s1.y_end_addr = sbuf_end_addr;
	s1.y_size = sbuf_size;
	s1.state = &cd->src.state1;
	s1.stage = cd->src.stage1;
	s1.x_rptr = src;
	s1.y_wptr = cd->sbuf_w_ptr;
	s1.nch = nch;

	s2.x_end_addr = sbuf_end_addr;
	s2.x_size = sbuf_size;
	s2.y_end_addr = sink->end_addr;
	s2.y_size = sink->size;
	s2.state = &cd->src.state2;
	s2.stage = cd->src.stage2;
	s2.x_rptr = cd->sbuf_r_ptr;
	s2.y_wptr = dest;
	s2.nch = nch;


	/* Test if 1st stage can be run with default block length to reach
	 * the period length or just under it.
	 */
	s1.times = cd->param.stage1_times;
	s1_blk_in = s1.times * cd->src.stage1->blk_in * nch;
	s1_blk_out = s1.times * cd->src.stage1->blk_out * nch;
	if ((avail_b >= s1_blk_in * sz) && (sbuf_free >= s1_blk_out)) {
		cd->polyphase_func(&s1);

		cd->sbuf_w_ptr = s1.y_wptr;
		cd->sbuf_avail += s1_blk_out;
		n_read += s1_blk_in;
		avail_b -= s1_blk_in * sz;
		sbuf_free -= s1_blk_out;
		n1 = s1.times;
	}

	/* Run one block at time the remaining data for 1st stage. */
	s1.times = 1;
	s1_blk_in = cd->src.stage1->blk_in * nch;
	s1_blk_out = cd->src.stage1->blk_out * nch;
	while ((n1 < cd->param.stage1_times_max) && (avail_b >= s1_blk_in * sz)
		&& (sbuf_free >= s1_blk_out)) {
		cd->polyphase_func(&s1);

		cd->sbuf_w_ptr = s1.y_wptr;
		cd->sbuf_avail += s1_blk_out;
		n_read += s1_blk_in;
		avail_b -= s1_blk_in * sz;
		sbuf_free -= s1_blk_out;
		n1 += s1.times;
	}

	/* Test if 2nd stage can be run with default block length. */
	s2.times = cd->param.stage2_times;
	s2_blk_in = s2.times * cd->src.stage2->blk_in * nch;
	s2_blk_out = s2.times * cd->src.stage2->blk_out * nch;
	if ((cd->sbuf_avail >= s2_blk_in) && (free_b >= s2_blk_out * sz)) {
		cd->polyphase_func(&s2);

		cd->sbuf_r_ptr = s2.x_rptr;
		cd->sbuf_avail -= s2_blk_in;
		free_b -= s2_blk_out * sz;
		n_written += s2_blk_out;
		n2 = s2.times;
	}


	/* Run one block at time the remaining 2nd stage output */
	s2.times = 1;
	s2_blk_in = cd->src.stage2->blk_in * nch;
	s2_blk_out = cd->src.stage2->blk_out * nch;
	while ((n2 < cd->param.stage2_times_max)
		&& (cd->sbuf_avail >= s2_blk_in)
		&& (free_b >= s2_blk_out * sz)) {
		cd->polyphase_func(&s2);

		cd->sbuf_r_ptr = s2.x_rptr;
		cd->sbuf_avail -= s2_blk_in;
		free_b -= s2_blk_out * sz;
		n_written += s2_blk_out;
		n2 += s2.times;
	}
	*bytes_read = sizeof(int32_t) * n_read;
	*bytes_written = sizeof(int32_t) * n_written;
}

/* 1 stage SRC for simple conversions */
static void src_1s_s32_default(struct comp_dev *dev,
	struct comp_buffer *source, struct comp_buffer *sink,
	size_t *bytes_read, size_t *bytes_written)
{
	struct src_stage_prm s1;
	struct comp_data *cd = comp_get_drvdata(dev);
	int nch = dev->params.channels;
	int n_read = 0;
	int n_written = 0;

	s1.times = cd->param.stage1_times;
	s1.x_rptr = (int32_t *) source->r_ptr;
	s1.x_end_addr = source->end_addr;
	s1.x_size = source->size;
	s1.y_wptr = (int32_t *) sink->w_ptr;
	s1.y_end_addr = sink->end_addr;
	s1.y_size = sink->size;
	s1.state = &cd->src.state1;
	s1.stage = cd->src.stage1;
	s1.nch = dev->params.channels;

	cd->polyphase_func(&s1);

	n_read += nch * cd->param.blk_in;
	n_written += nch * cd->param.blk_out;
	*bytes_read = n_read * sizeof(int32_t);
	*bytes_written = n_written * sizeof(int32_t);
}

/* A fast copy function for same in and out rate */
static void src_copy_s32_default(struct comp_dev *dev,
	struct comp_buffer *source, struct comp_buffer *sink,
	size_t *bytes_read, size_t *bytes_written)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *) source->r_ptr;
	int32_t *snk = (int32_t *) sink->w_ptr;
	int nch = dev->params.channels;
	int frames = cd->param.blk_in;
	int n;
	int n_wrap_src;
	int n_wrap_snk;
	int n_wrap_min;
	int n_copy;

	n = frames * nch;
	while (n > 0) {
		n_wrap_src = (int32_t *) source->end_addr - src;
		n_wrap_snk = (int32_t *) sink->end_addr - snk;
		n_wrap_min = (n_wrap_src < n_wrap_snk) ? n_wrap_src : n_wrap_snk;
		n_copy = (n < n_wrap_min) ? n : n_wrap_min;
		memcpy(snk, src, n_copy * sizeof(int32_t));

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src += n_copy;
		snk += n_copy;
		src_circ_inc_wrap(&src, source->end_addr, source->size);
		src_circ_inc_wrap(&snk, sink->end_addr, sink->size);

	}
	*bytes_read = frames * nch * sizeof(int32_t);
	*bytes_written = frames * nch * sizeof(int32_t);
}

static struct comp_dev *src_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_src *src;
	struct sof_ipc_comp_src *ipc_src = (struct sof_ipc_comp_src *) comp;
	struct comp_data *cd;

	trace_src("new");

	/* validate init data - either SRC sink or source rate must be set */
	if (ipc_src->source_rate == 0 && ipc_src->sink_rate == 0) {
		trace_src_error("sn1");
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		COMP_SIZE(struct sof_ipc_comp_src));
	if (dev == NULL)
		return NULL;

	src = (struct sof_ipc_comp_src *) &dev->comp;
	memcpy(src, ipc_src, sizeof(struct sof_ipc_comp_src));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (cd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->delay_lines = NULL;
	cd->src_func = src_2s_s32_default;
	cd->polyphase_func = src_polyphase_stage_cir;
	src_polyphase_reset(&cd->src);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void src_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_src("fre");

	/* Free dynamically reserved buffers for SRC algorithm */
	if (cd->delay_lines != NULL)
		rfree(cd->delay_lines);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int src_params(struct comp_dev *dev)
{
	struct sof_ipc_stream_params *params = &dev->params;
	struct sof_ipc_comp_src *src = COMP_GET_IPC(dev, sof_ipc_comp_src);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer *source;
	size_t delay_lines_size;
	uint32_t source_rate;
	uint32_t sink_rate;
	int32_t *buffer_start;
	int n = 0;
	int err;
	int frames_is_for_source;
	int q;

	trace_src("par");

	/* SRC supports S24_4LE and S32_LE formats */
	switch (config->frame_fmt) {
	case SOF_IPC_FRAME_S24_4LE:
		cd->polyphase_func = src_polyphase_stage_cir_s24;
		break;
	case SOF_IPC_FRAME_S32_LE:
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
	default:
		trace_src_error("sr0");
		return -EINVAL;
	}

	/* Calculate source and sink rates, one rate will come from IPC new
	 * and the other from params. */
	if (src->source_rate == 0) {
		/* params rate is source rate */
		source_rate = params->rate;
		sink_rate = src->sink_rate;
		/* re-write our params with output rate for next component */
		params->rate = sink_rate;
		frames_is_for_source = 0;
	} else {
		/* params rate is sink rate */
		source_rate = src->source_rate;
		sink_rate = params->rate;
		/* re-write our params with output rate for next component */
		params->rate = source_rate;
		frames_is_for_source = 1;
	}

	/* Allocate needed memory for delay lines */
	err = src_buffer_lengths(&cd->param, source_rate, sink_rate,
		params->channels, dev->frames, frames_is_for_source);
	if (err < 0) {
		trace_src_error("sr1");
		trace_value(source_rate);
		trace_value(sink_rate);
		trace_value(params->channels);
		trace_value(dev->frames);
		return err;
	}

	delay_lines_size = sizeof(int32_t) * cd->param.total;
	if (delay_lines_size == 0) {
		trace_src_error("sr2");
		return -EINVAL;
	}

	/* free any existing delay lines. TODO reuse if same size */
	if (cd->delay_lines != NULL)
		rfree(cd->delay_lines);

	cd->delay_lines = rballoc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		delay_lines_size);
	if (cd->delay_lines == NULL) {
		trace_src_error("sr3");
		trace_value(delay_lines_size);
		return -EINVAL;
	}

	/* Clear all delay lines here */
	memset(cd->delay_lines, 0, delay_lines_size);
	buffer_start = cd->delay_lines + cd->param.sbuf_length;

	/* Initialize SRC for actual sample rate */
	n = src_polyphase_init(&cd->src, &cd->param, buffer_start);

	/* Reset stage buffer */
	cd->sbuf_r_ptr = cd->delay_lines;
	cd->sbuf_w_ptr = cd->delay_lines;
	cd->sbuf_avail = 0;

	switch (n) {
	case 0:
		cd->src_func = src_copy_s32_default; /* 1:1 fast copy */
		break;
	case 1:
		cd->src_func = src_1s_s32_default; /* Simpler 1 stage SRC */
		break;
	case 2:
		cd->src_func = src_2s_s32_default; /* Default 2 stage SRC */
		break;
	default:
		/* This is possibly due to missing coefficients for
		 * requested rates combination. Sink audio will be
		 * muted if copy() is run.
		 */
		trace_src("SFa");
		cd->src_func = src_fallback;
		return -EINVAL;
		break;
	}

	/* Calculate period size based on config. First make sure that
	 * frame_bytes is set.
	 */
	dev->frame_bytes =
		dev->params.sample_container_bytes * dev->params.channels;

	/* The downstream buffer must be at least length of blk_out plus
	 * a dev->frames and an integer multiple of dev->frames. The
	 * buffer_set_size will return an error if the required length would
	 * be too long.
	 */
	q = src_ceil_divide(cd->param.blk_out, (int) dev->frames) + 1;

	/* Configure downstream buffer */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);
	err = buffer_set_size(sink, q * dev->frames * dev->frame_bytes);
	if (err < 0) {
		trace_src_error("eSz");
		trace_value(sink->alloc_size);
		trace_value(q * dev->frames * dev->frame_bytes);
		return err;
	}

	/* Check that source buffer has sufficient size */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
		sink_list);
	if (source->size < cd->param.blk_in * dev->frame_bytes) {
		trace_src_error("eSy");
		return -EINVAL;
	}


	return 0;
}

static int src_ctrl_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	trace_src_error("ec1");
	return -EINVAL;
}

/* used to pass standard and bespoke commands (with data) to component */
static int src_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_src("cmd");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (cmd == COMP_CMD_SET_VALUE)
		ret = src_ctrl_cmd(dev, cdata);

	return ret;
}

/* copy and process stream data from source to sink buffers */
static int src_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int need_source;
	int need_sink;
	size_t consumed = 0;
	size_t produced = 0;

	trace_src("SRC");

	/* src component needs 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
		sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);

	/* Calculate needed amount of source buffer and sink buffer
	 * for one SRC run. The blk_in and blk are minimum condition to
	 * call copy. Copy can consume or produce a slightly larger block
	 * with the rates where block sizes are not constant. E.g. for
	 * 1 ms scheduling the blocks can be under or above 1 ms when the
	 * SRC interval block size constraint prevents exact 1 ms blocks.
	 */
	need_source = cd->param.blk_in * dev->frame_bytes;
	need_sink = cd->param.blk_out * dev->frame_bytes;

	/* make sure source component buffer has enough data available and that
	 * the sink component buffer has enough free bytes for copy. Also
	 * check for XRUNs */
	if (source->avail < need_source) {
		trace_src_error("xru");
		return -EIO;	/* xrun */
	}
	if (sink->free < need_sink) {
		trace_src_error("xro");
		return -EIO;	/* xrun */
	}

	cd->src_func(dev, source, sink, &consumed, &produced);

	/* Calc new free and available if data was processed. These
	 * functions must not be called with 0 consumed/produced.
	 */
	if (consumed > 0)
		comp_update_buffer_consume(source, consumed);

	if (produced > 0) {
		comp_update_buffer_produce(sink, produced);
		return cd->param.blk_out;
	}

	/* produced no data */
	return 0;
}

static int src_prepare(struct comp_dev *dev)
{
	trace_src("pre");

	return comp_set_state(dev, COMP_CMD_PREPARE);
}

static int src_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_src("SRe");

	cd->src_func = src_2s_s32_default;
	src_polyphase_reset(&cd->src);

	comp_set_state(dev, COMP_CMD_RESET);
	return 0;
}

struct comp_driver comp_src = {
	.type = SOF_COMP_SRC,
	.ops = {
		.new = src_new,
		.free = src_free,
		.params = src_params,
		.cmd = src_cmd,
		.copy = src_copy,
		.prepare = src_prepare,
		.reset = src_reset,
	},
};

void sys_comp_src_init(void)
{
	comp_register(&comp_src);
}

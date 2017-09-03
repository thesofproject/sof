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
	struct polyphase_src src[PLATFORM_MAX_CHANNELS];
	int32_t *delay_lines;
	int scratch_length;
	uint32_t sink_rate;
	uint32_t source_rate;
	void (*src_func)(struct comp_dev *dev,
		struct comp_buffer *source,
		struct comp_buffer *sink,
		uint32_t source_frames,
		uint32_t sink_frames);
};

/* Common mute function for 2s and 1s SRC. This preserves the same
 * buffer consume and produce pattern as normal operation.
 */
static void src_muted_s32(struct comp_buffer *source, struct comp_buffer *sink,
	int blk_in, int blk_out, int nch, int source_frames)
{

	int i;
	int32_t *src = (int32_t *) source->r_ptr;
	int32_t *dest = (int32_t *) sink->w_ptr;
	int32_t *end = (int32_t *) sink->end_addr;
	int n_read = 0;
	int n_max;
	int n;
	int n_written = 0;

	for (i = 0; i < source_frames - blk_in + 1; i += blk_in) {
		n_max = end - dest;
		n = nch*blk_out;
		if (n < n_max) {
			bzero(dest, n * sizeof(int32_t));
			dest += n;
		} else {
			/* Also case n_max == n is done here */
			bzero(dest, n_max * sizeof(int32_t));
			dest = (int32_t *) sink->addr;
			bzero(dest, (n - n_max) * sizeof(int32_t));
			dest += n - n_max;
		}
		n_read += nch*blk_in;
		n_written += nch*blk_out;
	}
	source->r_ptr = src + n_read;
	sink->w_ptr = dest;
}

/* Fallback function to just output muted samples and advance
 * pointers. Note that a buffer that is not having integer number of
 * frames in a period will drift since there is no similar blk in/out
 * check as for SRC.
 */
static void fallback_s32(struct comp_dev *dev,
	struct comp_buffer *source,
	struct comp_buffer *sink,
	uint32_t source_frames,
	uint32_t sink_frames)
{

	struct comp_data *cd = comp_get_drvdata(dev);
	int nch = dev->params.channels;
	int blk_in = cd->src[0].blk_in;
	int blk_out = cd->src[0].blk_out;

	src_muted_s32(source, sink, blk_in, blk_out, nch, source_frames);

}

/* Normal 2 stage SRC */
static void src_2s_s32_default(struct comp_dev *dev,
	struct comp_buffer *source, struct comp_buffer *sink,
	uint32_t source_frames, uint32_t sink_frames)
{
	int i, j;
	struct polyphase_src *s;
	struct comp_data *cd = comp_get_drvdata(dev);
	int blk_in = cd->src[0].blk_in;
	int blk_out = cd->src[0].blk_out;
	int n_times1 = cd->src[0].stage1_times;
	int n_times2 = cd->src[0].stage2_times;
	int nch = dev->params.channels;
	int32_t *dest = (int32_t *) sink->w_ptr;
	int32_t *src = (int32_t *) source->r_ptr;
	struct src_stage_prm s1, s2;
	int n_read = 0;
	int n_written = 0;

	if (cd->src[0].mute) {
		src_muted_s32(source, sink, blk_in, blk_out, nch, source_frames);
		return;
	}

	s1.times = n_times1;
	s1.x_end_addr = source->end_addr;
	s1.x_size = source->alloc_size;
	s1.x_inc = nch;
	s1.y_end_addr = &cd->delay_lines[cd->scratch_length];
	s1.y_size = STAGE_BUF_SIZE * sizeof(int32_t);
	s1.y_inc = 1;

	s2.times = n_times2;
	s2.x_end_addr = &cd->delay_lines[cd->scratch_length];
	s2.x_size = STAGE_BUF_SIZE * sizeof(int32_t);
	s2.x_inc = 1;
	s2.y_end_addr = sink->end_addr;
	s2.y_size = sink->alloc_size;
	s2.y_inc = nch;

	s1.x_rptr = src + nch - 1;
	s2.y_wptr = dest + nch - 1;

	for (j = 0; j < nch; j++) {
		s = &cd->src[j]; /* Point to src[] for this channel */
		s1.x_rptr = src++;
		s2.y_wptr = dest++;
		s1.state = &s->state1;
		s1.stage = s->stage1;
		s2.state = &s->state2;
		s2.stage = s->stage2;

		for (i = 0; i < source_frames - blk_in + 1; i += blk_in) {
			/* Reset output to buffer start, read interleaved */
			s1.y_wptr = cd->delay_lines;
			src_polyphase_stage_cir(&s1);
			s2.x_rptr = cd->delay_lines;
			src_polyphase_stage_cir(&s2);

			n_read += blk_in;
			n_written += blk_out;
		}
	}
	source->r_ptr = s1.x_rptr - nch + 1;
	sink->w_ptr = s2.y_wptr - nch + 1;
}

/* 1 stage SRC for simple conversions */
static void src_1s_s32_default(struct comp_dev *dev,
	struct comp_buffer *source, struct comp_buffer *sink,
	uint32_t source_frames, uint32_t sink_frames)
{
	int i, j;
	//int32_t *xp, *yp;
	struct polyphase_src *s;

	struct comp_data *cd = comp_get_drvdata(dev);
	int blk_in = cd->src[0].blk_in;
	int blk_out = cd->src[0].blk_out;
	int n_times = cd->src[0].stage1_times;
	int nch = dev->params.channels;
	int32_t *dest = (int32_t *) sink->w_ptr;
	int32_t *src = (int32_t *) source->r_ptr;
	int n_read = 0;
	int n_written = 0;
	struct src_stage_prm s1;

	if (cd->src[0].mute) {
		src_muted_s32(source, sink, blk_in, blk_out, nch,
			source_frames);
		return;
	}

	s1.times = n_times;
	s1.x_end_addr = source->end_addr;
	s1.x_size = source->alloc_size;
	s1.x_inc = nch;
	s1.y_end_addr = sink->end_addr;
	s1.y_size = sink->alloc_size;
	s1.y_inc = nch;
	s1.x_rptr = src + nch - 1;
	s1.y_wptr = dest + nch - 1;

	for (j = 0; j < nch; j++) {
		s = &cd->src[j]; /* Point to src for this channel */
		s1.x_rptr = src++;
		s1.y_wptr = dest++;
		s1.state = &s->state1;
		s1.stage = s->stage1;

		for (i = 0; i + blk_in - 1 < source_frames; i += blk_in) {
			src_polyphase_stage_cir(&s1);

			n_read += blk_in;
			n_written += blk_out;
		}

	}
	source->r_ptr = s1.x_rptr - nch + 1;
	sink->w_ptr = s1.y_wptr - nch + 1;
}

static struct comp_dev *src_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_src *src;
	struct sof_ipc_comp_src *ipc_src = (struct sof_ipc_comp_src *) comp;
	struct comp_data *cd;
	int i;

	trace_src("new");

	/* validate init data - either SRC sink or source rate must be set */
	if (ipc_src->source_rate == 0 && ipc_src->sink_rate == 0) {
		trace_src_error("sn1");
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, RFLAGS_NONE,
		COMP_SIZE(struct sof_ipc_comp_src));
	if (dev == NULL)
		return NULL;

	src = (struct sof_ipc_comp_src *) &dev->comp;
	memcpy(src, ipc_src, sizeof(struct sof_ipc_comp_src));

	cd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*cd));
	if (cd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->delay_lines = NULL;
	cd->src_func = src_2s_s32_default;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		src_polyphase_reset(&cd->src[i]);

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
	struct src_alloc need;
	size_t delay_lines_size;
	uint32_t source_rate, sink_rate;
	int32_t *buffer_start;
	int n = 0, i, err;

	trace_src("par");

	/* SRC supports only S32_LE PCM format */
	if (config->frame_fmt != SOF_IPC_FRAME_S32_LE) {
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
	} else {
		/* params rate is sink rate */
		source_rate = src->source_rate;
		sink_rate = params->rate;
		/* re-write our params with output rate for next component */
		params->rate = source_rate;
	}

	/* Allocate needed memory for delay lines */
	err = src_buffer_lengths(&need, source_rate, sink_rate, params->channels);
	if (err < 0) {
		trace_src_error("sr1");
		trace_value(source_rate);
		trace_value(sink_rate);
		trace_value(params->channels);
		return err;
	}

	delay_lines_size = sizeof(int32_t) * need.total;
	if (delay_lines_size == 0) {
		trace_src_error("sr2");
		return -EINVAL;
	}

	/* free any existing dalay lines. TODO reuse if same size */
	if (cd->delay_lines != NULL)
		rfree(cd->delay_lines);

	cd->delay_lines = rballoc(RZONE_RUNTIME, RFLAGS_NONE, delay_lines_size);
	if (cd->delay_lines == NULL) {
		trace_src_error("sr3");
		trace_value(delay_lines_size);
		return -EINVAL;
	}

	/* Clear all delay lines here */
	memset(cd->delay_lines, 0, delay_lines_size);
	cd->scratch_length = need.scratch;
	buffer_start = cd->delay_lines + need.scratch;

	/* Initize SRC for actual sample rate */
	for (i = 0; i < params->channels; i++) {
		n = src_polyphase_init(&cd->src[i], source_rate,
			sink_rate, buffer_start);
		buffer_start += need.single_src;
	}

	switch (n) {
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
		cd->src_func = fallback_s32;
		return -EINVAL;
		break;
	}

	/* Need to compute this */
	dev->frame_bytes =
		dev->params.sample_container_bytes * dev->params.channels;

	return 0;
}

static int src_ctrl_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_MUTE:
		trace_src("SMu");
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			src_polyphase_mute(&cd->src[i]);

		break;
	case SOF_CTRL_CMD_UNMUTE:
		trace_src("SUm");
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			src_polyphase_unmute(&cd->src[i]);

		break;
	default:
		trace_src_error("ec1");
		return -EINVAL;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int src_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct sof_ipc_ctrl_data *cdata = data;

	trace_src("SCm");

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		return src_ctrl_cmd(dev, cdata);
	case COMP_CMD_START:
		trace_src("SSt");
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_STOP:
		trace_src("SSp");
		if (dev->state == COMP_STATE_RUNNING ||
			dev->state == COMP_STATE_DRAINING ||
			dev->state == COMP_STATE_PAUSED) {
			comp_buffer_reset(dev);
			dev->state = COMP_STATE_SETUP;
		}
		break;
	case COMP_CMD_PAUSE:
		trace_src("SPe");
		/* only support pausing for running */
		if (dev->state == COMP_STATE_RUNNING)
			dev->state = COMP_STATE_PAUSED;

		break;
	case COMP_CMD_RELEASE:
		trace_src("SRl");
		dev->state = COMP_STATE_RUNNING;
		break;
	default:
		trace_src("SDf");
		break;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int src_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source, *sink;
	int need_source, need_sink, blk_in, blk_out, frames_source, frames_sink;

	trace_comp("SRC");

	/* src component needs 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
		sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);

	/* Check that source has enough frames available and sink enough
	 * frames free.
	 */
	blk_in = src_polyphase_get_blk_in(&cd->src[0]);
	blk_out = src_polyphase_get_blk_out(&cd->src[0]);
	frames_source = dev->frames * blk_in / blk_out;
	frames_sink = frames_source * blk_out / blk_in;
	need_source = frames_source * dev->frame_bytes;
	need_sink = frames_sink * dev->frame_bytes;

	/* Run as many times as buffers allow */
	while ((source->avail >= need_source) && (sink->free >= need_sink)) {
		/* Run src */
		cd->src_func(dev, source, sink, frames_source, frames_sink);

		/* calc new free and available  */
		comp_update_buffer_consume(source, 0);
		comp_update_buffer_produce(sink, 0);
	}

	return 0;
}

static int src_prepare(struct comp_dev *dev)
{
	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static int src_preload(struct comp_dev *dev)
{
	return src_copy(dev);
}

static int src_reset(struct comp_dev *dev)
{
	int i;
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_src("SRe");

	cd->src_func = src_2s_s32_default;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		src_polyphase_reset(&cd->src[i]);

	dev->state = COMP_STATE_INIT;
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
		.preload = src_preload,
	},
};

void sys_comp_src_init(void)
{
	comp_register(&comp_src);
}

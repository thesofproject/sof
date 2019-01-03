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
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/work.h>
#include <sof/clk.h>
#include <sof/ipc.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/math/numbers.h>
#include <uapi/ipc/topology.h>

#include "src_config.h"
#include "src.h"

#if SRC_SHORT
#include <sof/audio/coefficients/src/src_tiny_int16_define.h>
#include <sof/audio/coefficients/src/src_tiny_int16_table.h>
#else
#include <sof/audio/coefficients/src/src_std_int32_define.h>
#include <sof/audio/coefficients/src/src_std_int32_table.h>
#endif

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#define trace_src(__e, ...) trace_event(TRACE_CLASS_SRC, __e, ##__VA_ARGS__)
#define tracev_src(__e, ...) tracev_event(TRACE_CLASS_SRC, __e, ##__VA_ARGS__)
#define trace_src_error(__e, ...) trace_error(TRACE_CLASS_SRC, __e, ##__VA_ARGS__)

/* The FIR maximum lengths are per channel so need to multiply them */
#define MAX_FIR_DELAY_SIZE_XNCH (PLATFORM_MAX_CHANNELS * MAX_FIR_DELAY_SIZE)
#define MAX_OUT_DELAY_SIZE_XNCH (PLATFORM_MAX_CHANNELS * MAX_OUT_DELAY_SIZE)

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
	int prefill;
	int data_shift;
	void (*src_func)(struct comp_dev *dev,
			 struct comp_buffer *source,
			 struct comp_buffer *sink,
			 int *consumed,
			 int *produced);
	void (*polyphase_func)(struct src_stage_prm *s);
};

/* Calculates the needed FIR delay line length */
static int src_fir_delay_length(struct src_stage *s)
{
	return s->subfilter_length + (s->num_of_subfilters - 1) * s->idm
		+ s->blk_in;
}

/* Calculates the FIR output delay line length */
static int src_out_delay_length(struct src_stage *s)
{
	return 1 + (s->num_of_subfilters - 1) * s->odm;
}

/* Returns index of a matching sample rate */
static int src_find_fs(int fs_list[], int list_length, int fs)
{
	int i;

	for (i = 0; i < list_length; i++) {
		if (fs_list[i] == fs)
			return i;
	}
	return -EINVAL;
}

/* Calculates buffers to allocate for a SRC mode */
int src_buffer_lengths(struct src_param *a, int fs_in, int fs_out, int nch,
		       int frames, int frames_is_for_source)
{
	struct src_stage *stage1;
	struct src_stage *stage2;
	int q;
	int den;
	int num;
	int frames2;

	if (nch > PLATFORM_MAX_CHANNELS) {
		trace_src_error("src_buffer_lengths() error: "
				"nch = %u > PLATFORM_MAX_CHANNELS", nch);
		return -EINVAL;
	}

	a->nch = nch;
	a->idx_in = src_find_fs(src_in_fs, NUM_IN_FS, fs_in);
	a->idx_out = src_find_fs(src_out_fs, NUM_OUT_FS, fs_out);

	/* Check that both in and out rates are supported */
	if (a->idx_in < 0 || a->idx_out < 0) {
		trace_src_error("src_buffer_lengths() error: "
				"rates not supported, "
				"fs_in: %u, fs_out: %u", fs_in, fs_out);
		return -EINVAL;
	}

	stage1 = src_table1[a->idx_out][a->idx_in];
	stage2 = src_table2[a->idx_out][a->idx_in];

	/* Check from stage1 parameter for a deleted in/out rate combination.*/
	if (stage1->filter_length < 1) {
		trace_src_error("src_buffer_lengths() error: "
				"stage1->filter_length <"
				" 1, fs_in: %u, fs_out: %u", fs_in, fs_out);
		return -EINVAL;
	}

	a->fir_s1 = nch * src_fir_delay_length(stage1);
	a->out_s1 = nch * src_out_delay_length(stage1);

	/* Find out how many additional times the SRC can be executed
	 * while having block size less or equal to max_frames.
	 */
	if (frames_is_for_source) {
		/* Times that stage1 needs to run to input length of frames */
		a->stage1_times_max = ceil_divide(frames, stage1->blk_in);
		q = frames / stage1->blk_in;
		a->stage1_times = MAX(q, 1);
		a->blk_in = a->stage1_times * stage1->blk_in;

		/* Times that stage2 needs to run */
		den = stage2->blk_in * stage1->blk_in;
		num = frames * stage2->blk_out * stage1->blk_out;
		frames2 = ceil_divide(num, den);
		a->stage2_times_max = ceil_divide(frames2, stage2->blk_out);
		q = frames2 / stage2->blk_out;
		a->stage2_times = MAX(q, 1);
		a->blk_out = a->stage2_times * stage2->blk_out;
	} else {
		/* Times that stage2 needs to run to output length of frames */
		a->stage2_times_max = ceil_divide(frames, stage2->blk_out);
		q = frames / stage2->blk_out;
		a->stage2_times = MAX(q, 1);
		a->blk_out = a->stage2_times * stage2->blk_out;

		/* Times that stage1 needs to run */
		num = frames * stage2->blk_in * stage1->blk_in;
		den = stage2->blk_out * stage1->blk_out;
		frames2 = ceil_divide(num, den);
		a->stage1_times_max = ceil_divide(frames2, stage1->blk_in);
		q = frames2 / stage1->blk_in;
		a->stage1_times = MAX(q, 1);
		a->blk_in = a->stage1_times * stage1->blk_in;
	}

	if (stage2->filter_length == 1) {
		a->fir_s2 = 0;
		a->out_s2 = 0;
		a->stage2_times = 0;
		a->stage2_times_max = 0;
		a->sbuf_length = 0;
	} else {
		a->fir_s2 = nch * src_fir_delay_length(stage2);
		a->out_s2 = nch * src_out_delay_length(stage2);
		/* 2x is an empirically tested length. Since the sink buffer
		 * capability to receive samples varies a shorter stage 2 output
		 * block will create a peak in internal buffer usage.
		 */

		/* TODO 1: Equation for needed length */
		a->sbuf_length = 2 * nch * stage1->blk_out
			* a->stage1_times_max;
	}

	a->src_multich = a->fir_s1 + a->fir_s2 + a->out_s1 + a->out_s2;
	a->total = a->sbuf_length + a->src_multich;

	return 0;
}

static void src_state_reset(struct src_state *state)
{
	state->fir_delay_size = 0;
	state->out_delay_size = 0;
}

static int init_stages(struct src_stage *stage1, struct src_stage *stage2,
		       struct polyphase_src *src, struct src_param *p,
		       int n, int32_t *delay_lines_start)
{
	/* Clear FIR state */
	src_state_reset(&src->state1);
	src_state_reset(&src->state2);

	src->number_of_stages = n;
	src->stage1 = stage1;
	src->stage2 = stage2;
	if (n == 1 && stage1->blk_out == 0)
		return -EINVAL;

	/* Optimized SRC requires subfilter length multiple of 4 */
	if (stage1->filter_length > 1 && (stage1->subfilter_length & 0x3) > 0)
		return -EINVAL;

	if (stage2->filter_length > 1 && (stage2->subfilter_length & 0x3) > 0)
		return -EINVAL;

	/* Delay line sizes */
	src->state1.fir_delay_size = p->fir_s1;
	src->state1.out_delay_size = p->out_s1;
	src->state1.fir_delay = delay_lines_start;
	src->state1.out_delay =
		src->state1.fir_delay + src->state1.fir_delay_size;
	/* Initialize to last ensures that circular wrap cannot happen
	 * mid-frame. The size is multiple of channels count.
	 */
	src->state1.fir_wp = &src->state1.fir_delay[p->fir_s1 - 1];
	src->state1.out_rp = src->state1.out_delay;
	if (n > 1) {
		src->state2.fir_delay_size = p->fir_s2;
		src->state2.out_delay_size = p->out_s2;
		src->state2.fir_delay =
			src->state1.out_delay + src->state1.out_delay_size;
		src->state2.out_delay =
			src->state2.fir_delay + src->state2.fir_delay_size;
		/* Initialize to last ensures that circular wrap cannot happen
		 * mid-frame. The size is multiple of channels count.
		 */
		src->state2.fir_wp = &src->state2.fir_delay[p->fir_s2 - 1];
		src->state2.out_rp = src->state2.out_delay;
	} else {
		src->state2.fir_delay_size = 0;
		src->state2.out_delay_size = 0;
		src->state2.fir_delay = NULL;
		src->state2.out_delay = NULL;
	}

	/* Check the sizes are less than MAX */
	if (src->state1.fir_delay_size > MAX_FIR_DELAY_SIZE_XNCH ||
	    src->state1.out_delay_size > MAX_OUT_DELAY_SIZE_XNCH ||
	    src->state2.fir_delay_size > MAX_FIR_DELAY_SIZE_XNCH ||
	    src->state2.out_delay_size > MAX_OUT_DELAY_SIZE_XNCH) {
		src->state1.fir_delay = NULL;
		src->state1.out_delay = NULL;
		src->state2.fir_delay = NULL;
		src->state2.out_delay = NULL;
		return -EINVAL;
	}

	return 0;
}

void src_polyphase_reset(struct polyphase_src *src)
{
	src->number_of_stages = 0;
	src->stage1 = NULL;
	src->stage2 = NULL;
	src_state_reset(&src->state1);
	src_state_reset(&src->state2);
}

int src_polyphase_init(struct polyphase_src *src, struct src_param *p,
		       int32_t *delay_lines_start)
{
	struct src_stage *stage1;
	struct src_stage *stage2;
	int n_stages;
	int ret;

	if (p->idx_in < 0 || p->idx_out < 0)
		return -EINVAL;

	/* Get setup for 2 stage conversion */
	stage1 = src_table1[p->idx_out][p->idx_in];
	stage2 = src_table2[p->idx_out][p->idx_in];
	ret = init_stages(stage1, stage2, src, p, 2, delay_lines_start);
	if (ret < 0)
		return -EINVAL;

	/* Get number of stages used for optimize opportunity. 2nd
	 * stage length is one if conversion needs only one stage.
	 * If input and output rate is the same return 0 to
	 * use a simple copy function instead of 1 stage FIR with one
	 * tap.
	 */
	n_stages = (src->stage2->filter_length == 1) ? 1 : 2;
	if (p->idx_in == p->idx_out)
		n_stages = 0;

	/* If filter length for first stage is zero this is a deleted
	 * mode from in/out matrix. Computing of such SRC mode needs
	 * to be prevented.
	 */
	if (src->stage1->filter_length == 0)
		return -EINVAL;

	return n_stages;
}

/* Fallback function */
static void src_fallback(struct comp_dev *dev, struct comp_buffer *source,
			 struct comp_buffer *sink, int *n_read, int *n_written)
{
	*n_read = 0;
	*n_written = 0;
}

/* Normal 2 stage SRC */
static void src_2s(struct comp_dev *dev,
		   struct comp_buffer *source, struct comp_buffer *sink,
		   int *n_read, int *n_written)
{
	struct src_stage_prm s1;
	struct src_stage_prm s2;
	int s1_blk_in;
	int s1_blk_out;
	int s2_blk_in;
	int s2_blk_out;
	struct comp_data *cd = comp_get_drvdata(dev);
	void *sbuf_addr = cd->delay_lines;
	void *sbuf_end_addr = &cd->delay_lines[cd->param.sbuf_length];
	size_t sbuf_size = cd->param.sbuf_length * sizeof(int32_t);
	int nch = dev->params.channels;
	int sbuf_free = cd->param.sbuf_length - cd->sbuf_avail;
	int n1 = 0;
	int n2 = 0;
	int avail_b = source->avail;
	int free_b = sink->free;
	int sz = dev->params.sample_container_bytes;

	*n_read = 0;
	*n_written = 0;
	s1.x_end_addr = source->end_addr;
	s1.x_size = source->size;
	s1.y_addr = sbuf_addr;
	s1.y_end_addr = sbuf_end_addr;
	s1.y_size = sbuf_size;
	s1.state = &cd->src.state1;
	s1.stage = cd->src.stage1;
	s1.x_rptr = source->r_ptr;
	s1.y_wptr = cd->sbuf_w_ptr;
	s1.nch = nch;
	s1.shift = cd->data_shift;

	s2.x_end_addr = sbuf_end_addr;
	s2.x_size = sbuf_size;
	s2.y_addr = sink->addr;
	s2.y_end_addr = sink->end_addr;
	s2.y_size = sink->size;
	s2.state = &cd->src.state2;
	s2.stage = cd->src.stage2;
	s2.x_rptr = cd->sbuf_r_ptr;
	s2.y_wptr = sink->w_ptr;
	s2.nch = nch;
	s2.shift = cd->data_shift;

	/* Test if 1st stage can be run with default block length to reach
	 * the period length or just under it.
	 */
	s1.times = cd->param.stage1_times;
	s1_blk_in = s1.times * cd->src.stage1->blk_in * nch;
	s1_blk_out = s1.times * cd->src.stage1->blk_out * nch;
	if (avail_b >= s1_blk_in * sz && sbuf_free >= s1_blk_out) {
		cd->polyphase_func(&s1);

		cd->sbuf_w_ptr = s1.y_wptr;
		cd->sbuf_avail += s1_blk_out;
		*n_read += s1.times * cd->src.stage1->blk_in;
		avail_b -= s1_blk_in * sz;
		sbuf_free -= s1_blk_out;
		n1 = s1.times;
	}

	/* Run one block at time the remaining data for 1st stage. */
	s1.times = 1;
	s1_blk_in = cd->src.stage1->blk_in * nch;
	s1_blk_out = cd->src.stage1->blk_out * nch;
	while (n1 < cd->param.stage1_times_max &&
	       avail_b >= s1_blk_in * sz &&
	       sbuf_free >= s1_blk_out) {
		cd->polyphase_func(&s1);

		cd->sbuf_w_ptr = s1.y_wptr;
		cd->sbuf_avail += s1_blk_out;
		*n_read += cd->src.stage1->blk_in;
		avail_b -= s1_blk_in * sz;
		sbuf_free -= s1_blk_out;
		n1 += s1.times;
	}

	/* Test if 2nd stage can be run with default block length. */
	s2.times = cd->param.stage2_times;
	s2_blk_in = s2.times * cd->src.stage2->blk_in * nch;
	s2_blk_out = s2.times * cd->src.stage2->blk_out * nch;
	if (cd->sbuf_avail >= s2_blk_in && free_b >= s2_blk_out * sz) {
		cd->polyphase_func(&s2);

		cd->sbuf_r_ptr = s2.x_rptr;
		cd->sbuf_avail -= s2_blk_in;
		free_b -= s2_blk_out * sz;
		*n_written += s2.times * cd->src.stage2->blk_out;
		n2 = s2.times;
	}

	/* Run one block at time the remaining 2nd stage output */
	s2.times = 1;
	s2_blk_in = cd->src.stage2->blk_in * nch;
	s2_blk_out = cd->src.stage2->blk_out * nch;
	while (n2 < cd->param.stage2_times_max &&
	       cd->sbuf_avail >= s2_blk_in &&
	       free_b >= s2_blk_out * sz) {
		cd->polyphase_func(&s2);

		cd->sbuf_r_ptr = s2.x_rptr;
		cd->sbuf_avail -= s2_blk_in;
		free_b -= s2_blk_out * sz;
		*n_written += cd->src.stage2->blk_out;
		n2 += s2.times;
	}
}

/* 1 stage SRC for simple conversions */
static void src_1s(struct comp_dev *dev,
		   struct comp_buffer *source, struct comp_buffer *sink,
		   int *n_read, int *n_written)
{
	struct src_stage_prm s1;
	struct comp_data *cd = comp_get_drvdata(dev);

	s1.times = cd->param.stage1_times;
	s1.x_rptr = source->r_ptr;
	s1.x_end_addr = source->end_addr;
	s1.x_size = source->size;
	s1.y_wptr = sink->w_ptr;
	s1.y_end_addr = sink->end_addr;
	s1.y_size = sink->size;
	s1.state = &cd->src.state1;
	s1.stage = cd->src.stage1;
	s1.nch = dev->params.channels;
	s1.shift = cd->data_shift;

	cd->polyphase_func(&s1);

	*n_read = cd->param.blk_in;
	*n_written = cd->param.blk_out;
}

/* A fast copy function for same in and out rate */
static void src_copy_s32(struct comp_dev *dev,
			 struct comp_buffer *source, struct comp_buffer *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int nch = dev->params.channels;
	int frames = cd->param.blk_in;
	int n;
	int n_wrap_src;
	int n_wrap_snk;
	int n_wrap_min;
	int n_copy;

	n = frames * nch;
	while (n > 0) {
		n_wrap_src = (int32_t *)source->end_addr - src;
		n_wrap_snk = (int32_t *)sink->end_addr - snk;
		n_wrap_min = (n_wrap_src < n_wrap_snk) ?
			n_wrap_src : n_wrap_snk;
		n_copy = (n < n_wrap_min) ? n : n_wrap_min;
		memcpy(snk, src, n_copy * sizeof(int32_t));

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src += n_copy;
		snk += n_copy;
		src_inc_wrap(&src, source->end_addr, source->size);
		src_inc_wrap(&snk, sink->end_addr, sink->size);
	}
	*n_read = frames;
	*n_written = frames;
}

static void src_copy_s16(struct comp_dev *dev,
			 struct comp_buffer *source, struct comp_buffer *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int16_t *snk = (int16_t *)sink->w_ptr;
	int nch = dev->params.channels;
	int frames = cd->param.blk_in;
	int n;
	int n_wrap_src;
	int n_wrap_snk;
	int n_wrap_min;
	int n_copy;

	n = frames * nch;
	while (n > 0) {
		n_wrap_src = (int16_t *)source->end_addr - src;
		n_wrap_snk = (int16_t *)sink->end_addr - snk;
		n_wrap_min = (n_wrap_src < n_wrap_snk) ?
			n_wrap_src : n_wrap_snk;
		n_copy = (n < n_wrap_min) ? n : n_wrap_min;
		memcpy(snk, src, n_copy * sizeof(int16_t));

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src += n_copy;
		snk += n_copy;
		src_inc_wrap_s16(&src, source->end_addr, source->size);
		src_inc_wrap_s16(&snk, sink->end_addr, sink->size);
	}
	*n_read = frames;
	*n_written = frames;
}

static struct comp_dev *src_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_src *src;
	struct sof_ipc_comp_src *ipc_src = (struct sof_ipc_comp_src *)comp;
	struct comp_data *cd;

	trace_src("src_new()");

	if (IPC_IS_SIZE_INVALID(ipc_src->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_SRC, ipc_src->config);
		return NULL;
	}

	/* validate init data - either SRC sink or source rate must be set */
	if (ipc_src->source_rate == 0 && ipc_src->sink_rate == 0) {
		trace_src_error("src_new() error: "
				"SRC sink and source rate are not set");
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_src));
	if (!dev)
		return NULL;

	src = (struct sof_ipc_comp_src *)&dev->comp;
	memcpy(src, ipc_src, sizeof(struct sof_ipc_comp_src));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->delay_lines = NULL;
	cd->src_func = src_fallback;
	cd->polyphase_func = src_polyphase_stage_cir;
	src_polyphase_reset(&cd->src);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void src_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_src("src_free()");

	/* Free dynamically reserved buffers for SRC algorithm */
	if (!cd->delay_lines)
		rfree(cd->delay_lines);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int src_params(struct comp_dev *dev)
{
	struct sof_ipc_stream_params *params = &dev->params;
	struct sof_ipc_comp_src *src = COMP_GET_IPC(dev, sof_ipc_comp_src);
	struct comp_data *cd = comp_get_drvdata(dev);
	size_t delay_lines_size;
	int32_t *buffer_start;
	int n = 0;
	int err;
	int frames_is_for_source;

	trace_src("src_params()");

	/* Calculate source and sink rates, one rate will come from IPC new
	 * and the other from params.
	 */
	if (src->source_rate == 0) {
		/* params rate is source rate */
		cd->source_rate = params->rate;
		cd->sink_rate = src->sink_rate;
		/* re-write our params with output rate for next component */
		params->rate = cd->sink_rate;
		frames_is_for_source = 0;
	} else {
		/* params rate is sink rate */
		cd->source_rate = src->source_rate;
		cd->sink_rate = params->rate;
		/* re-write our params with output rate for next component */
		params->rate = cd->source_rate;
		frames_is_for_source = 1;
	}

	/* Allocate needed memory for delay lines */
	trace_src("src_params(), source_rate = %u, sink_rate = %u",
		  cd->source_rate, cd->sink_rate);
	trace_src("src_params(), params->channels = %u, dev->frames = %u",
		  params->channels, dev->frames);
	err = src_buffer_lengths(&cd->param, cd->source_rate, cd->sink_rate,
				 params->channels, dev->frames,
				 frames_is_for_source);
	if (err < 0) {
		trace_src_error(
			"src_params() error: src_buffer_lengths() failed");
		return err;
	}

	trace_src("src_params(), blk_in = %u, blk_out = %u",
		  cd->param.blk_in, cd->param.blk_out);

	delay_lines_size = sizeof(int32_t) * cd->param.total;
	if (delay_lines_size == 0) {
		trace_src_error("src_params() error: delay_lines_size = 0");
		return -EINVAL;
	}

	/* free any existing delay lines. TODO reuse if same size */
	if (!cd->delay_lines)
		rfree(cd->delay_lines);

	cd->delay_lines = rballoc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
				  delay_lines_size);
	if (!cd->delay_lines) {
		trace_src_error("src_params() error: "
				"failed to alloc cd->delay_lines, "
				"delay_lines_size = %u", delay_lines_size);
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
		/* 1:1 fast copy */
		cd->src_func = src_copy_s32;
		break;
	case 1:
		cd->src_func = src_1s; /* Simpler 1 stage SRC */
		break;
	case 2:
		cd->src_func = src_2s; /* Default 2 stage SRC */
		break;
	default:
		/* This is possibly due to missing coefficients for
		 * requested rates combination. Sink audio will be
		 * muted if copy() is run.
		 */
		trace_src("src_params(), missing coefficients "
			  "for requested rates combination");
		cd->src_func = src_fallback;
		return -EINVAL;
	}

	return 0;
}

static int src_ctrl_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	trace_src_error("src_ctrl_cmd()");
	return -EINVAL;
}

/* used to pass standard and bespoke commands (with data) to component */
static int src_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_src("src_cmd()");

	if (cmd == COMP_CMD_SET_VALUE)
		ret = src_ctrl_cmd(dev, cdata);

	return ret;
}

static int src_trigger(struct comp_dev *dev, int cmd)
{
	trace_src("src_trigger()");

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int src_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int need_source;
	int need_sink;
	int consumed = 0;
	int produced = 0;

	tracev_src("src_copy()");

	/* src component needs 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* In some conversions the first copy run needs to pre-fill buffer
	 * with sufficient amount of zeros if the min. output block length
	 * is too short. It prevents xrun for the downstream component. In
	 * successive copy executions the block length will jitter around the
	 * nominal period length and xruns won't happen.
	 */
	if (cd->prefill && sink->free >= cd->prefill) {
		tracev_src("src_copy(), need to "
			   "pre-fill buffer, cd->prefill = %u", cd->prefill);
		comp_update_buffer_produce(sink, cd->prefill);
		cd->prefill = 0;
	}

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
	 * check for XRUNs.
	 */
	if (source->avail < need_source) {
		trace_src_error("src_copy() error: source component buffer "
				"has not enough data available");
		return -EIO; /* xrun */
	}
	if (sink->free < need_sink) {
		trace_src_error("src_copy() error: sink component buffer "
				"has not enough free bytes for copy");
		return -EIO; /* xrun */
	}

	cd->src_func(dev, source, sink, &consumed, &produced);

	tracev_src("src_copy(), consumed = %u,  produced = %u",
		   consumed, produced);

	/* Calc new free and available if data was processed. These
	 * functions must not be called with 0 consumed/produced.
	 */
	if (consumed > 0)
		comp_update_buffer_consume(source, consumed * dev->frame_bytes);

	if (produced > 0) {
		comp_update_buffer_produce(sink, produced * dev->frame_bytes);
		return produced;
	}

	/* produced no data */
	return 0;
}

static int src_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer *source;
	int q;
	int d;
	int err;

	trace_src("src_prepare()");

	/* SRC supports S16_LE, S24_4LE and S32_LE formats */
	switch (dev->params.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		cd->data_shift = 0;
		cd->polyphase_func = src_polyphase_stage_cir_s16;
		/* Copy function is set by default in params() for 32 bit
		 * data. Change it to 16 bit version here if source and sink
		 * rates are equal.
		 */
		if (cd->source_rate == cd->sink_rate) {
			cd->src_func = src_copy_s16;
		}
		break;
	case SOF_IPC_FRAME_S24_4LE:
		cd->data_shift = 8;
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
	case SOF_IPC_FRAME_S32_LE:
		cd->data_shift = 0;
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
	default:
		trace_src_error("src_prepare() error: "
				"invalid config->frame_fmt");
		return -EINVAL;
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
	q = ceil_divide(cd->param.blk_out, (int)dev->frames) + 1;

	/* If conversion specific minimum period length is less than
	 * default period length there is need to pre-fill into sink buffer
	 * zero PCM samples.
	 */
	d = dev->frames - cd->param.blk_out;
	cd->prefill = (d > 0) ? d * dev->frame_bytes : 0;

	/* Configure downstream buffer */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);
	err = buffer_set_size(sink, q * dev->frames * dev->frame_bytes);
	if (err < 0) {
		trace_src_error("src_params() error: buffer_set_size() failed,"
				" sink->alloc_size = %u, (q * dev->frames * "
				"dev->frame_bytes) = %u", sink->alloc_size,
				q * dev->frames * dev->frame_bytes);
		return err;
	}

	/* Check that source buffer has sufficient size */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	if (source->size < cd->param.blk_in * dev->frame_bytes) {
		trace_src_error("src_params() error: source->size < "
				"cd->param.blk_in * dev->frame_bytes");
		return -EINVAL;
	}

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static int src_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_src("src_reset()");

	cd->src_func = src_fallback;
	src_polyphase_reset(&cd->src);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static void src_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;

	switch (cmd) {
	case COMP_CACHE_WRITEBACK_INV:
		trace_src("src_cache(), COMP_CACHE_WRITEBACK_INV");

		cd = comp_get_drvdata(dev);

		if (cd->delay_lines)
			dcache_writeback_invalidate_region
				(cd->delay_lines,
				 sizeof(int32_t) * cd->param.total);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case COMP_CACHE_INVALIDATE:
		trace_src("src_cache(), COMP_CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));

		if (cd->delay_lines)
			dcache_invalidate_region
				(cd->delay_lines,
				 sizeof(int32_t) * cd->param.total);
		break;
	}
}

struct comp_driver comp_src = {
	.type = SOF_COMP_SRC,
	.ops = {
		.new = src_new,
		.free = src_free,
		.params = src_params,
		.cmd = src_cmd,
		.trigger = src_trigger,
		.copy = src_copy,
		.prepare = src_prepare,
		.reset = src_reset,
		.cache = src_cache,
	},
};

void sys_comp_src_init(void)
{
	comp_register(&comp_src);
}

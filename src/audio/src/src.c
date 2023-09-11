// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/common.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_source_utils.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/base-config.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "src.h"
#include "src_config.h"

#if SRC_SHORT || CONFIG_COMP_SRC_TINY
#include "coef/src_tiny_int16_define.h"
#include "coef/src_tiny_int16_table.h"
#elif CONFIG_COMP_SRC_SMALL
#include "coef/src_small_int32_define.h"
#include "coef/src_small_int32_table.h"
#elif CONFIG_COMP_SRC_STD
#include "coef/src_std_int32_define.h"
#include "coef/src_std_int32_table.h"
#elif CONFIG_COMP_SRC_IPC4_FULL_MATRIX
#include "coef/src_ipc4_int32_define.h"
#include "coef/src_ipc4_int32_table.h"
#else
#error "No valid configuration selected for SRC"
#endif

/* The FIR maximum lengths are per channel so need to multiply them */
#define MAX_FIR_DELAY_SIZE_XNCH (PLATFORM_MAX_CHANNELS * MAX_FIR_DELAY_SIZE)
#define MAX_OUT_DELAY_SIZE_XNCH (PLATFORM_MAX_CHANNELS * MAX_OUT_DELAY_SIZE)

LOG_MODULE_REGISTER(src, CONFIG_SOF_LOG_LEVEL);

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
static int src_buffer_lengths(struct comp_dev *dev, struct comp_data *cd,
			      int nch)
{
	struct src_stage *stage1;
	struct src_stage *stage2;
	struct src_param *a;
	int fs_in, fs_out;
	int source_frames;
	int r1, n;

	a = &cd->param;
	fs_in = cd->source_rate;
	fs_out = cd->sink_rate;
	source_frames = cd->source_frames;

	if (nch > PLATFORM_MAX_CHANNELS) {
		/* TODO: should be device, not class */
		comp_err(dev, "src_buffer_lengths(): nch = %u > PLATFORM_MAX_CHANNELS",
			 nch);
		return -EINVAL;
	}

	a->nch = nch;
	a->idx_in = src_find_fs(src_in_fs, NUM_IN_FS, fs_in);
	a->idx_out = src_find_fs(src_out_fs, NUM_OUT_FS, fs_out);

	/* Check that both in and out rates are supported */
	if (a->idx_in < 0 || a->idx_out < 0) {
		comp_err(dev, "src_buffer_lengths(): rates not supported, fs_in: %u, fs_out: %u",
			 fs_in, fs_out);
		return -EINVAL;
	}

	stage1 = src_table1[a->idx_out][a->idx_in];
	stage2 = src_table2[a->idx_out][a->idx_in];

	/* Check from stage1 parameter for a deleted in/out rate combination.*/
	if (stage1->filter_length < 1) {
		comp_err(dev, "src_buffer_lengths(): Non-supported combination sfs_in = %d, fs_out = %d",
			 fs_in, fs_out);
		return -EINVAL;
	}

	a->fir_s1 = nch * src_fir_delay_length(stage1);
	a->out_s1 = nch * src_out_delay_length(stage1);

	/* Computing of number of blocks to process is done in
	 * copy() per each frame.
	 */
	a->stage1_times = 0;
	a->stage2_times = 0;
	a->blk_in = 0;
	a->blk_out = 0;

	if (stage2->filter_length == 1) {
		a->fir_s2 = 0;
		a->out_s2 = 0;
		a->sbuf_length = 0;
	} else {
		a->fir_s2 = nch * src_fir_delay_length(stage2);
		a->out_s2 = nch * src_out_delay_length(stage2);

		/* Stage 1 is repeated max. amount that just exceeds one
		 * period.
		 */
		r1 = source_frames / stage1->blk_in + 1;

		/* Set sbuf length to allow storing two stage 1 output
		 * periods. This is an empirically found value for no
		 * xruns to happen with SRC in/out buffers. Due to
		 * variable number of blocks to process per each stage
		 * there is no equation known for minimum size.
		 */
		n = 2 * stage1->blk_out * r1;
		a->sbuf_length = nch * (n + (n >> 3));
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
	if (src_in_fs[p->idx_in] == src_out_fs[p->idx_out])
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
int src_fallback(struct comp_data *cd, struct sof_source *source,
		 struct sof_sink *sink)
{
	return 0;
}

/* Normal 2 stage SRC */
static int src_2s(struct comp_data *cd,
		  struct sof_source *source, struct sof_sink *sink)
{
	struct src_stage_prm s1;
	struct src_stage_prm s2;
	int s1_blk_in;
	int s1_blk_out;
	int s2_blk_in;
	int s2_blk_out;
	uint32_t n_read = 0, n_written = 0;
	int ret;
	uint8_t const *source_buffer_start;
	uint8_t *sink_buffer_start;
	void *sbuf_end_addr = &cd->delay_lines[cd->param.sbuf_length];
	size_t sbuf_size = cd->param.sbuf_length * sizeof(int32_t);
	/* chan sink == chan src therefore we only need to use one*/
	int nch = source_get_channels(source);
	int sbuf_free = cd->param.sbuf_length - cd->sbuf_avail;
	int avail_b = source_get_data_available(source);
	int free_b = sink_get_free_size(sink);
	int sz = cd->sample_container_bytes;
	uint32_t source_frag_size = cd->param.blk_in * source_get_frame_bytes(source);
	uint32_t sink_frag_size = cd->param.blk_out * sink_get_frame_bytes(sink);

	ret = source_get_data(source, source_frag_size,
			      &s1.x_rptr, (void const **)&source_buffer_start, &s1.x_size);
	if (ret)
		return ret;
	s1.x_end_addr = source_buffer_start + s1.x_size;

	ret = sink_get_buffer(sink, sink_frag_size,
			      &s2.y_wptr, (void **)&sink_buffer_start, &s2.y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}
	s2.y_end_addr = sink_buffer_start + s2.y_size;

	s1.y_end_addr = sbuf_end_addr;
	s1.y_size = sbuf_size;
	s1.state = &cd->src.state1;
	s1.stage = cd->src.stage1;
	s1.y_wptr = cd->sbuf_w_ptr;
	s1.nch = nch;
	s1.shift = cd->data_shift;

	s2.x_end_addr = sbuf_end_addr;
	s2.x_size = sbuf_size;
	s2.state = &cd->src.state2;
	s2.stage = cd->src.stage2;
	s2.x_rptr = cd->sbuf_r_ptr;
	s2.nch = nch;
	s2.shift = cd->data_shift;

	/* Test if 1st stage can be run with default block length to reach
	 * the period length or just under it.
	 */
	s1.times = cd->param.stage1_times;
	s1_blk_out = s1.times * cd->src.stage1->blk_out * nch;

	/* The sbuf may limit how many times s1 can be looped. It's harder
	 * to prepare for in advance so the repeats number is adjusted down
	 * here if need.
	 */
	if (s1_blk_out > sbuf_free) {
		s1.times = sbuf_free / (cd->src.stage1->blk_out * nch);
		s1_blk_out = s1.times * cd->src.stage1->blk_out * nch;
	}

	s1_blk_in = s1.times * cd->src.stage1->blk_in * nch;
	if (avail_b >= s1_blk_in * sz && sbuf_free >= s1_blk_out) {
		cd->polyphase_func(&s1);

		cd->sbuf_w_ptr = s1.y_wptr;
		cd->sbuf_avail += s1_blk_out;
		n_read += s1.times * cd->src.stage1->blk_in;
	}

	s2.times = cd->param.stage2_times;
	s2_blk_in = s2.times * cd->src.stage2->blk_in * nch;
	if (s2_blk_in > cd->sbuf_avail) {
		s2.times = cd->sbuf_avail / (cd->src.stage2->blk_in * nch);
		s2_blk_in = s2.times * cd->src.stage2->blk_in * nch;
	}

	/* Test if second stage can be run with default block length. */
	s2_blk_out = s2.times * cd->src.stage2->blk_out * nch;
	if (cd->sbuf_avail >= s2_blk_in && free_b >= s2_blk_out * sz) {
		cd->polyphase_func(&s2);

		cd->sbuf_r_ptr = s2.x_rptr;
		cd->sbuf_avail -= s2_blk_in;
		n_written += s2.times * cd->src.stage2->blk_out;
	}

	/* commit the processed data */
	source_release_data(source, n_read * source_get_frame_bytes(source));
	sink_commit_buffer(sink, n_written * sink_get_frame_bytes(sink));
	return 0;
}

/* 1 stage SRC for simple conversions */
static int src_1s(struct comp_data *cd, struct sof_source *source,
		  struct sof_sink *sink)
{
	struct src_stage_prm s1;
	int ret;
	uint8_t const *source_buffer_start;
	uint8_t *sink_buffer_start;
	uint32_t source_frag_size = cd->param.blk_in * source_get_frame_bytes(source);
	uint32_t sink_frag_size = cd->param.blk_out * sink_get_frame_bytes(sink);

	ret = source_get_data(source, source_frag_size,
			      &s1.x_rptr, (void const **)&source_buffer_start, &s1.x_size);
	if (ret)
		return ret;
	s1.x_end_addr = source_buffer_start + s1.x_size;

	ret = sink_get_buffer(sink, sink_frag_size,
			      &s1.y_wptr, (void **)&sink_buffer_start, &s1.y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}
	s1.y_end_addr = sink_buffer_start + s1.y_size;

	s1.times = cd->param.stage1_times;
	s1.state = &cd->src.state1;
	s1.stage = cd->src.stage1;
	s1.nch = source_get_channels(source);	/* src channels must == sink channels */
	s1.shift = cd->data_shift;

	cd->polyphase_func(&s1);

	/* commit the processed data */
	source_release_data(source, UINT_MAX);
	sink_commit_buffer(sink, UINT_MAX);

	return 0;
}

/* A fast copy function for same in and out rate */
static int src_copy_sxx(struct comp_data *cd, struct sof_source *source,
			struct sof_sink *sink)
{
	int frames = cd->param.blk_in;

	switch (source_get_frm_fmt(source)) {
	case SOF_IPC_FRAME_S16_LE:
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		return source_to_sink_copy(source, sink, true,
					   frames * source_get_frame_bytes(source));
	default:
		break;
	}

	return -ENOTSUP;
}

void src_set_alignment(struct sof_source *source, struct sof_sink *sink)
{
	const uint32_t byte_align = 1;
	const uint32_t frame_align_req = 1;

	source_set_alignment_constants(source, byte_align, frame_align_req);
	sink_set_alignment_constants(sink, byte_align, frame_align_req);
}

static int src_verify_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "src_verify_params()");

	/* check whether params->rate (received from driver) are equal
	 * to src->source_rate (PLAYBACK) or src->sink_rate (CAPTURE) set during
	 * creating src component in src_new().
	 * src->source/sink_rate = 0 means that source/sink rate can vary.
	 */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		ret = src_stream_pcm_sink_rate_check(cd->ipc_config, params);
		if (ret < 0) {
			comp_err(dev, "src_verify_params(): sink stream rate does not match rate fetched from ipc.");
			return ret;
		}
	} else {
		ret = src_stream_pcm_source_rate_check(cd->ipc_config, params);
		if (ret < 0) {
			comp_err(dev, "src_verify_params(): source stream rate does not match rate fetched from ipc.");
			return ret;
		}
	}

	/* update downstream (playback) or upstream (capture) buffer parameters
	 */
	ret = comp_verify_params(dev, BUFF_PARAMS_RATE, params);
	if (ret < 0)
		comp_err(dev, "src_verify_params(): comp_verify_params() failed.");

	return ret;
}

static bool src_get_copy_limits(struct comp_data *cd,
				struct sof_source *source,
				struct sof_sink *sink)
{
	struct src_param *sp;
	struct src_stage *s1;
	struct src_stage *s2;
	int frames_src;
	int frames_snk;

	/* Get SRC parameters */
	sp = &cd->param;
	s1 = cd->src.stage1;
	s2 = cd->src.stage2;

	/* Calculate how many blocks can be processed with
	 * available source and free sink frames amount.
	 */
	frames_snk = sink_get_free_frames(sink);
	frames_src = source_get_data_frames_available(source);
	if (s2->filter_length > 1) {
		/* Two polyphase filters case */
		frames_snk = MIN(frames_snk, cd->sink_frames + s2->blk_out);
		sp->stage2_times = frames_snk / s2->blk_out;
		frames_src = MIN(frames_src, cd->source_frames + s1->blk_in);
		sp->stage1_times = frames_src / s1->blk_in;
		sp->blk_in = sp->stage1_times * s1->blk_in;
		sp->blk_out = sp->stage2_times * s2->blk_out;
	} else {
		/* Single polyphase filter case */
		frames_snk = MIN(frames_snk, cd->sink_frames + s1->blk_out);
		sp->stage1_times = frames_snk / s1->blk_out;
		sp->stage1_times = MIN(sp->stage1_times,
				       frames_src / s1->blk_in);
		sp->blk_in = sp->stage1_times * s1->blk_in;
		sp->blk_out = sp->stage1_times * s1->blk_out;
	}

	if (sp->blk_in == 0 && sp->blk_out == 0)
		return false;

	return true;
}

static int src_params_general(struct processing_module *mod,
			      struct sof_source *source,
			      struct sof_sink *sink)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	size_t delay_lines_size;
	int32_t *buffer_start;
	int n;
	int err;

	comp_info(dev, "src_params()");

	err = src_set_params(mod, sink);
	if (err < 0) {
		comp_err(dev, "src_params(): set params failed.");
		return err;
	}

	err = src_verify_params(mod);
	if (err < 0) {
		comp_err(dev, "src_params(): pcm params verification failed.");
		return err;
	}

	src_get_source_sink_params(dev, source, sink);

	comp_info(dev, "src_params(), source_rate = %u, sink_rate = %u",
		  cd->source_rate, cd->sink_rate);
	comp_dbg(dev, "src_params(), sample_container_bytes = %d, channels = %u, dev->frames = %u",
		 cd->sample_container_bytes, cd->channels_count, dev->frames);
	if (!cd->sink_rate) {
		comp_err(dev, "src_params(), zero sink rate");
		return  -EINVAL;
	}

	cd->source_frames = dev->frames * cd->source_rate / cd->sink_rate;
	cd->sink_frames = dev->frames;

	/* Allocate needed memory for delay lines */
	err = src_buffer_lengths(dev, cd, cd->channels_count);
	if (err < 0) {
		comp_err(dev, "src_params(): src_buffer_lengths() failed");
		return err;
	}

	/*
	 * delay_lines_size is used to compute buffer_start which needs to
	 * be aligned to 8 bytes as required by some Xtensa
	 * instructions (e.g AE_L32X2F24_XC)
	 */
	delay_lines_size = ALIGN_UP(sizeof(int32_t) * cd->param.total, 8);
	if (delay_lines_size == 0) {
		comp_err(dev, "src_params(): delay_lines_size = 0");

		return  -EINVAL;
	}

	/* free any existing delay lines. TODO reuse if same size */
	rfree(cd->delay_lines);

	cd->delay_lines = rballoc(0, SOF_MEM_CAPS_RAM, delay_lines_size);
	if (!cd->delay_lines) {
		comp_err(dev, "src_params(): failed to alloc cd->delay_lines, delay_lines_size = %u",
			 delay_lines_size);
		return  -EINVAL;
	}

	/* Clear all delay lines here */
	memset(cd->delay_lines, 0, delay_lines_size);
	buffer_start = cd->delay_lines + ALIGN_UP(cd->param.sbuf_length, 2);

	/* Initialize SRC for actual sample rate */
	n = src_polyphase_init(&cd->src, &cd->param, buffer_start);

	/* Reset stage buffer */
	cd->sbuf_r_ptr = cd->delay_lines;
	cd->sbuf_w_ptr = cd->delay_lines;
	cd->sbuf_avail = 0;

	switch (n) {
	case 0:
		/* 1:1 fast copy */
		cd->src_func = src_copy_sxx;
		break;
	case 1:
		cd->src_func = src_1s; /* Simpler 1 stage SRC */
		break;
	case 2:
		cd->src_func = src_2s; /* Default 2 stage SRC */
		break;
	default:
		/* This is possibly due to missing coefficients for
		 * requested rates combination.
		 */
		comp_info(dev, "src_params(), missing coefficients for requested rates combination");
		cd->src_func = src_fallback;
		return  -EINVAL;
	}

	return 0;
}

static int src_prepare(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	int ret;

	comp_info(mod->dev, "src_prepare()");

	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	ret = src_params_general(mod, sources[0], sinks[0]);
	if (ret < 0)
		return ret;

	return src_prepare_general(mod, sources[0], sinks[0]);
}


static bool src_is_ready_to_process(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);

	return src_get_copy_limits(cd, sources[0], sinks[0]);
}

static int src_process(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "src_process()");

	/* src component needs 1 source and 1 sink */
	if (!src_get_copy_limits(cd, sources[0], sinks[0])) {
		comp_dbg(mod->dev, "No data to process.");
		return 0;
	}

	return cd->src_func(cd, sources[0], sinks[0]);
}

static int src_set_config(struct processing_module *mod, uint32_t config_id,
			  enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			  const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			  size_t response_size)
{
	return -EINVAL;
}

static int src_get_config(struct processing_module *mod, uint32_t config_id,
			  uint32_t *data_offset_size, uint8_t *fragment, size_t fragment_size)
{
	return -EINVAL;
}

static int src_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "src_reset()");

	cd->src_func = src_fallback;
	src_polyphase_reset(&cd->src);

	return 0;
}

static int src_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "src_free()");

	/* Free dynamically reserved buffers for SRC algorithm */
	rfree(cd->delay_lines);

	rfree(cd);
	return 0;
}

static const struct module_interface src_interface = {
	.init = src_init,
	.prepare = src_prepare,
	.process = src_process,
	.is_ready_to_process = src_is_ready_to_process,
	.set_configuration = src_set_config,
	.get_configuration = src_get_config,
	.reset = src_reset,
	.free = src_free,
};

DECLARE_MODULE_ADAPTER(src_interface, src_uuid, src_tr);
SOF_MODULE_INIT(src, sys_comp_module_src_interface_init);

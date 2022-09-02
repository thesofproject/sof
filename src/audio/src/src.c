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
#include <sof/audio/src/src.h>
#include <sof/audio/src/src_config.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
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

#if SRC_SHORT || CONFIG_COMP_SRC_TINY
#include <sof/audio/coefficients/src/src_tiny_int16_define.h>
#include <sof/audio/coefficients/src/src_tiny_int16_table.h>
#elif CONFIG_COMP_SRC_SMALL
#include <sof/audio/coefficients/src/src_small_int32_define.h>
#include <sof/audio/coefficients/src/src_small_int32_table.h>
#elif CONFIG_COMP_SRC_STD
#include <sof/audio/coefficients/src/src_std_int32_define.h>
#include <sof/audio/coefficients/src/src_std_int32_table.h>
#elif CONFIG_COMP_SRC_IPC4_FULL_MATRIX
#include <sof/audio/coefficients/src/src_ipc4_int32_define.h>
#include <sof/audio/coefficients/src/src_ipc4_int32_table.h>
#else
#error "No valid configuration selected for SRC"
#endif

/* The FIR maximum lengths are per channel so need to multiply them */
#define MAX_FIR_DELAY_SIZE_XNCH (PLATFORM_MAX_CHANNELS * MAX_FIR_DELAY_SIZE)
#define MAX_OUT_DELAY_SIZE_XNCH (PLATFORM_MAX_CHANNELS * MAX_OUT_DELAY_SIZE)

static const struct comp_driver comp_src;

LOG_MODULE_REGISTER(src, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_IPC_MAJOR_4
/* e61bb28d-149a-4c1f-b709-46823ef5f5a3 */
DECLARE_SOF_RT_UUID("src", src_uuid, 0xe61bb28d, 0x149a, 0x4c1f,
		    0xb7, 0x09, 0x46, 0x82, 0x3e, 0xf5, 0xf5, 0xae);
#elif CONFIG_IPC_MAJOR_3
/* c1c5326d-8390-46b4-aa47-95c3beca6550 */
DECLARE_SOF_RT_UUID("src", src_uuid, 0xc1c5326d, 0x8390, 0x46b4,
		    0xaa, 0x47, 0x95, 0xc3, 0xbe, 0xca, 0x65, 0x50);
#else
#error "No or invalid IPC MAJOR version selected."
#endif /* CONFIG_IPC_MAJOR_4 */

DECLARE_TR_CTX(src_tr, SOF_UUID(src_uuid), LOG_LEVEL_INFO);

/* src component private data */
struct ipc4_config_src {
	struct ipc4_base_module_cfg base;
	uint32_t sink_rate;
};

struct comp_data {
#if CONFIG_IPC_MAJOR_4
	struct ipc4_config_src ipc_config;
#else
	struct ipc_config_src ipc_config;
#endif /* CONFIG_IPC_MAJOR_4 */
	struct polyphase_src src;
	struct src_param param;
	int32_t *delay_lines;
	uint32_t sink_rate;
	uint32_t source_rate;
	int32_t *sbuf_w_ptr;
	int32_t *sbuf_r_ptr;
	int sbuf_avail;
	int data_shift;
	int source_frames;
	int sink_frames;
	int sample_container_bytes;
	void (*src_func)(struct comp_dev *dev,
			 const struct audio_stream __sparse_cache *source,
			 struct audio_stream __sparse_cache *sink,
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
		       int source_frames)
{
	struct src_stage *stage1;
	struct src_stage *stage2;
	int r1;

	if (nch > PLATFORM_MAX_CHANNELS) {
		/* TODO: should be device, not class */
		comp_cl_err(&comp_src, "src_buffer_lengths(): nch = %u > PLATFORM_MAX_CHANNELS",
			    nch);
		return -EINVAL;
	}

	a->nch = nch;
	a->idx_in = src_find_fs(src_in_fs, NUM_IN_FS, fs_in);
	a->idx_out = src_find_fs(src_out_fs, NUM_OUT_FS, fs_out);

	/* Check that both in and out rates are supported */
	if (a->idx_in < 0 || a->idx_out < 0) {
		comp_cl_err(&comp_src, "src_buffer_lengths(): rates not supported, fs_in: %u, fs_out: %u",
			    fs_in, fs_out);
		return -EINVAL;
	}

	stage1 = src_table1[a->idx_out][a->idx_in];
	stage2 = src_table2[a->idx_out][a->idx_in];

	/* Check from stage1 parameter for a deleted in/out rate combination.*/
	if (stage1->filter_length < 1) {
		comp_cl_err(&comp_src, "src_buffer_lengths(): Non-supported combination sfs_in = %d, fs_out = %d",
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
		a->sbuf_length = 2 * nch * stage1->blk_out * r1;
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
static void src_fallback(struct comp_dev *dev,
			 const struct audio_stream __sparse_cache *source,
			 struct audio_stream __sparse_cache *sink, int *n_read, int *n_written)
{
	*n_read = 0;
	*n_written = 0;
}

/* Normal 2 stage SRC */
static void src_2s(struct comp_dev *dev, const struct audio_stream __sparse_cache *source,
		   struct audio_stream __sparse_cache *sink, int *n_read, int *n_written)
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
	int nch = source->channels;
	int sbuf_free = cd->param.sbuf_length - cd->sbuf_avail;
	int avail_b = audio_stream_get_avail_bytes(source);
	int free_b = audio_stream_get_free_bytes(sink);
	int sz = cd->sample_container_bytes;

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

	/* The sbuf may limit how many times s1 can be looped. It's harder
	 * to prepare for in advance so the repeats number is adjusted down
	 * here if need.
	 */
	if (s1_blk_out > sbuf_free) {
		s1.times = sbuf_free / (cd->src.stage1->blk_out * nch);
		s1_blk_in = s1.times * cd->src.stage1->blk_in * nch;
		s1_blk_out = s1.times * cd->src.stage1->blk_out * nch;
		comp_dbg(dev, "s1.times = %d", s1.times);
	}

	if (avail_b >= s1_blk_in * sz && sbuf_free >= s1_blk_out) {
		cd->polyphase_func(&s1);

		cd->sbuf_w_ptr = s1.y_wptr;
		cd->sbuf_avail += s1_blk_out;
		*n_read += s1.times * cd->src.stage1->blk_in;
	}

	s2.times = cd->param.stage2_times;
	s2_blk_in = s2.times * cd->src.stage2->blk_in * nch;
	s2_blk_out = s2.times * cd->src.stage2->blk_out * nch;
	if (s2_blk_in > cd->sbuf_avail) {
		s2.times = cd->sbuf_avail / (cd->src.stage2->blk_in * nch);
		s2_blk_in = s2.times * cd->src.stage2->blk_in * nch;
		s2_blk_out = s2.times * cd->src.stage2->blk_out * nch;
		comp_dbg(dev, "s2.times = %d", s2.times);
	}

	/* Test if second stage can be run with default block length. */
	if (cd->sbuf_avail >= s2_blk_in && free_b >= s2_blk_out * sz) {
		cd->polyphase_func(&s2);

		cd->sbuf_r_ptr = s2.x_rptr;
		cd->sbuf_avail -= s2_blk_in;
		*n_written += s2.times * cd->src.stage2->blk_out;
	}
}

/* 1 stage SRC for simple conversions */
static void src_1s(struct comp_dev *dev, const struct audio_stream __sparse_cache *source,
		   struct audio_stream __sparse_cache *sink, int *n_read, int *n_written)
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
	s1.nch = source->channels;
	s1.shift = cd->data_shift;

	cd->polyphase_func(&s1);

	*n_read = cd->param.blk_in;
	*n_written = cd->param.blk_out;
}

/* A fast copy function for same in and out rate */
static void src_copy_sxx(struct comp_dev *dev,
			 const struct audio_stream __sparse_cache *source,
			 struct audio_stream __sparse_cache *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int frames = cd->param.blk_in;

	switch (sink->frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		audio_stream_copy(source, 0, sink, 0,
				  frames * source->channels);
		*n_read = frames;
		*n_written = frames;
		break;
	default:
		*n_read = 0;
		*n_written = 0;
	}
}

#if CONFIG_IPC_MAJOR_4
static int src_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = cd->ipc_config.base;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int src_rate_check(void *spec)
{
	struct ipc4_config_src *ipc_src = spec;

	if (ipc_src->base.audio_fmt.sampling_frequency == 0 && ipc_src->sink_rate == 0)
		return -EINVAL;

	return 0;
}

static int src_stream_pcm_source_rate_check(struct ipc4_config_src cfg,
					    struct sof_ipc_stream_params *params)
{
	if (cfg.base.audio_fmt.sampling_frequency &&
	    params->rate != cfg.base.audio_fmt.sampling_frequency)
		return -EINVAL;

	return 0;
}

/* In ipc4 case param is figured out by module config so we need to first
 * set up param then verify param. BTW for IPC3 path, the param is sent by
 * host driver.
 */
static void src_set_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;

	memset(params, 0, sizeof(*params));
	params->channels = cd->ipc_config.base.audio_fmt.channels_count;
	params->rate = cd->ipc_config.base.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->ipc_config.base.audio_fmt.depth / 8;
	params->sample_valid_bytes = cd->ipc_config.base.audio_fmt.valid_bit_depth / 8;
	params->frame_fmt = dev->ipc_config.frame_fmt;
	params->buffer_fmt = cd->ipc_config.base.audio_fmt.interleaving_style;
	params->buffer.size = cd->ipc_config.base.ibs;

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sinkb->stream.rate = cd->ipc_config.sink_rate;
}

static void src_set_sink_params(struct comp_dev *dev, struct comp_buffer __sparse_cache *sinkb)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	/* convert IPC4 config to format used by the module */
	audio_stream_fmt_conversion(cd->ipc_config.base.audio_fmt.depth,
				    cd->ipc_config.base.audio_fmt.valid_bit_depth,
				    &sinkb->stream.frame_fmt,
				    &sinkb->stream.valid_sample_fmt,
				    cd->ipc_config.base.audio_fmt.s_type);
	sinkb->stream.channels = cd->ipc_config.base.audio_fmt.channels_count;
	sinkb->buffer_fmt = cd->ipc_config.base.audio_fmt.interleaving_style;
}

#elif CONFIG_IPC_MAJOR_3
static int src_rate_check(void *spec)
{
	struct ipc_config_src *ipc_src = spec;

	if (ipc_src->source_rate == 0 && ipc_src->sink_rate == 0)
		return -EINVAL;

	return 0;
}

static int src_stream_pcm_source_rate_check(struct ipc_config_src cfg,
					    struct sof_ipc_stream_params *params)
{
	if (cfg.source_rate && params->rate != cfg.source_rate)
		return -EINVAL;

	return 0;
}

static void src_set_params(struct comp_dev *dev, struct sof_ipc_stream_params *params) {}

static void src_set_sink_params(struct comp_dev *dev, struct comp_buffer __sparse_cache *sinkb) {}
#else
#error "No or invalid IPC MAJOR version selected."
#endif /* CONFIG_IPC_MAJOR_4 */

static struct comp_dev *src_new(const struct comp_driver *drv,
				struct comp_ipc_config *config,
				void *spec)
{
	struct comp_dev *dev;
	struct comp_data *cd;

	comp_cl_info(&comp_src, "src_new()");

	/* validate init data - either SRC sink or source rate must be set */
	if (src_rate_check(spec) < 0) {
		comp_cl_err(&comp_src, "src_new(): SRC sink and source rate are not set");
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	memcpy_s(&cd->ipc_config, sizeof(cd->ipc_config), spec, sizeof(cd->ipc_config));

	cd->delay_lines = NULL;
	cd->src_func = src_fallback;
	cd->polyphase_func = NULL;
	src_polyphase_reset(&cd->src);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void src_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "src_free()");

	/* Free dynamically reserved buffers for SRC algorithm */
	if (cd->delay_lines)
		rfree(cd->delay_lines);

	rfree(cd);
	rfree(dev);
}

static int src_verify_params(struct comp_dev *dev,
			     struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	comp_dbg(dev, "src_verify_params()");

	/* check whether params->rate (received from driver) are equal
	 * to src->source_rate (PLAYBACK) or src->sink_rate (CAPTURE) set during
	 * creating src component in src_new().
	 * src->source/sink_rate = 0 means that source/sink rate can vary.
	 */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		ret = src_stream_pcm_source_rate_check(cd->ipc_config, params);
		if (ret < 0) {
			comp_err(dev, "src_verify_params(): runtime stream pcm rate does not match rate fetched from ipc.");
			return ret;
		}
	} else {
		if (cd->ipc_config.sink_rate && (params->rate != cd->ipc_config.sink_rate)) {
			comp_err(dev, "src_verify_params(): runtime stream pcm rate %u does not match rate %u fetched from ipc.",
				 params->rate, cd->ipc_config.sink_rate);
			return -EINVAL;
		}
	}

	/* update downstream (playback) or upstream (capture) buffer parameters
	 */
	ret = comp_verify_params(dev, BUFF_PARAMS_RATE, params);
	if (ret < 0) {
		comp_err(dev, "src_verify_params(): comp_verify_params() failed.");
	}

	return ret;
}

/* set component audio stream parameters */
static int src_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	size_t delay_lines_size;
	int32_t *buffer_start;
	int n;
	int err;

	comp_info(dev, "src_params()");

	src_set_params(dev, params);
	err = src_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "src_params(): pcm params verification failed.");
		return -EINVAL;
	}

	cd->sample_container_bytes = params->sample_container_bytes;

	/* src components will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	source_c = buffer_acquire(sourceb);
	sink_c = buffer_acquire(sinkb);

	src_set_sink_params(dev, sink_c);

	/* Set source/sink_rate/frames */
	cd->source_rate = source_c->stream.rate;
	cd->sink_rate = sink_c->stream.rate;
	if (!cd->sink_rate) {
		comp_err(dev, "src_params(), zero sink rate");

		err = -EINVAL;
		goto out;
	}

	cd->source_frames = dev->frames * cd->source_rate / cd->sink_rate;
	cd->sink_frames = dev->frames;

	/* Allocate needed memory for delay lines */
	comp_info(dev, "src_params(), source_rate = %u, sink_rate = %u, format = %d",
		  cd->source_rate, cd->sink_rate, source_c->stream.frame_fmt);
	comp_info(dev, "src_params(), sourceb->channels = %u, sinkb->channels = %u, dev->frames = %u",
		  source_c->stream.channels, sink_c->stream.channels, dev->frames);
	err = src_buffer_lengths(&cd->param, cd->source_rate, cd->sink_rate,
				 source_c->stream.channels, cd->source_frames);
	if (err < 0) {
		comp_err(dev, "src_params(): src_buffer_lengths() failed");
		goto out;
	}

	delay_lines_size = sizeof(int32_t) * cd->param.total;
	if (delay_lines_size == 0) {
		comp_err(dev, "src_params(): delay_lines_size = 0");

		err = -EINVAL;
		goto out;
	}

	/* free any existing delay lines. TODO reuse if same size */
	if (cd->delay_lines)
		rfree(cd->delay_lines);

	cd->delay_lines = rballoc(0, SOF_MEM_CAPS_RAM, delay_lines_size);
	if (!cd->delay_lines) {
		comp_err(dev, "src_params(): failed to alloc cd->delay_lines, delay_lines_size = %u",
			 delay_lines_size);
		err = -EINVAL;
		goto out;
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
		err = -EINVAL;
	}

out:
	buffer_release(sink_c);
	buffer_release(source_c);

	return err;
}

static int src_ctrl_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	comp_err(dev, "src_ctrl_cmd()");
	return -EINVAL;
}

/* used to pass standard and bespoke commands (with data) to component */
static int src_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "src_cmd()");

	if (cmd == COMP_CMD_SET_VALUE)
		ret = src_ctrl_cmd(dev, cdata);

	return ret;
}

static int src_trigger(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "src_trigger()");

	if (cmd == COMP_TRIGGER_START || cmd == COMP_TRIGGER_RELEASE) {
		if (!cd->polyphase_func) {
			comp_err(dev, "polyphase_func is not set");
			return -EINVAL;
		}
	}
	return comp_set_state(dev, cmd);
}

static int src_get_copy_limits(struct comp_data *cd,
			       const struct comp_buffer __sparse_cache *source,
			       const struct comp_buffer __sparse_cache *sink)
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
	if (s2->filter_length > 1) {
		/* Two polyphase filters case */
		frames_snk = audio_stream_get_free_frames(&sink->stream);
		frames_snk = MIN(frames_snk, cd->sink_frames + s2->blk_out);
		sp->stage2_times = frames_snk / s2->blk_out;
		frames_src = audio_stream_get_avail_frames(&source->stream);
		frames_src = MIN(frames_src, cd->source_frames + s1->blk_in);
		sp->stage1_times = frames_src / s1->blk_in;
		sp->blk_in = sp->stage1_times * s1->blk_in;
		sp->blk_out = sp->stage2_times * s2->blk_out;
	} else {
		/* Single polyphase filter case */
		frames_snk = audio_stream_get_free_frames(&sink->stream);
		frames_snk = MIN(frames_snk, cd->sink_frames + s1->blk_out);
		sp->stage1_times = frames_snk / s1->blk_out;
		frames_src = audio_stream_get_avail_frames(&source->stream);
		sp->stage1_times = MIN(sp->stage1_times,
				       frames_src / s1->blk_in);
		sp->blk_in = sp->stage1_times * s1->blk_in;
		sp->blk_out = sp->stage1_times * s1->blk_out;
	}

	if (sp->blk_in == 0 || sp->blk_out == 0)
		return -EIO;

	return 0;
}

static void src_process(struct comp_dev *dev, struct comp_buffer __sparse_cache *source,
			struct comp_buffer __sparse_cache *sink)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int consumed = 0;
	int produced = 0;

	/* consumed bytes are not known at this point */
	buffer_stream_invalidate(source, source->stream.size);
	cd->src_func(dev, &source->stream, &sink->stream, &consumed, &produced);
	buffer_stream_writeback(sink, produced * audio_stream_frame_bytes(&sink->stream));

	comp_dbg(dev, "src_copy(), consumed = %u,  produced = %u",
		 consumed, produced);

	comp_update_buffer_consume(source, consumed *
				   audio_stream_frame_bytes(&source->stream));
	comp_update_buffer_produce(sink, produced *
				   audio_stream_frame_bytes(&sink->stream));
}

/* copy and process stream data from source to sink buffers */
static int src_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	int ret;

	comp_dbg(dev, "src_copy()");

	/* src component needs 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	source_c = buffer_acquire(source);
	sink_c = buffer_acquire(sink);

	/* Get from buffers and SRC conversion specific block constraints
	 * how many frames can be processed. If sufficient number of samples
	 * is not available the processing is omitted.
	 */
	ret = src_get_copy_limits(cd, source_c, sink_c);
	if (ret)
		comp_dbg(dev, "No data to process.");
	else
		src_process(dev, source_c, sink_c);

	buffer_release(sink_c);
	buffer_release(source_c);

	return 0;
}

static int src_check_buffer_sizes(struct comp_data *cd,
				  struct audio_stream __sparse_cache *source_stream,
				  struct audio_stream __sparse_cache *sink_stream)
{
	struct src_stage *s1 = cd->src.stage1;
	struct src_stage *s2 = cd->src.stage2;
	int stage1_times;
	int stage2_times;
	int blk_in;
	int blk_out;
	int n;

	if (s2->filter_length > 1) {
		/* Two polyphase filters case */
		stage2_times = ceil_divide(cd->sink_frames, s2->blk_out);
		stage1_times = ceil_divide(cd->source_frames, s1->blk_in);
		blk_in = stage1_times * s1->blk_in;
		blk_out = stage2_times * s2->blk_out;
	} else {
		/* Single polyphase filter case */
		stage1_times = ceil_divide(cd->sink_frames, s1->blk_out);
		n = ceil_divide(cd->source_frames, s1->blk_in);
		stage1_times = MAX(stage1_times, n);
		blk_in = stage1_times * s1->blk_in;
		blk_out = stage1_times * s1->blk_out;
	}

	n = audio_stream_frame_bytes(source_stream) * (blk_in + cd->source_frames);
	if (source_stream->size < n) {
		comp_cl_err(&comp_src, "Source size %d is less than required %d",
			source_stream->size, n);
		return -ENOBUFS;
	}

	n = audio_stream_frame_bytes(sink_stream) * (blk_out + cd->sink_frames);
	if (sink_stream->size < n) {
		comp_cl_err(&comp_src, "Sink size %d is less than required %d",
			sink_stream->size, n);
		return -ENOBUFS;
	}

	return 0;
}

static int src_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	int ret;

	comp_info(dev, "src_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* SRC component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	source_c = buffer_acquire(sourceb);
	sink_c = buffer_acquire(sinkb);

	/* get source data format */
	source_format = source_c->stream.frame_fmt;

	/* get sink data format */
	sink_format = sink_c->stream.frame_fmt;

	ret = src_check_buffer_sizes(cd, &source_c->stream, &sink_c->stream);
	if (ret < 0)
		goto out;

	/* SRC supports S16_LE, S24_4LE and S32_LE formats */
	if (source_format != sink_format) {
		comp_err(dev, "src_prepare(): Source fmt %d and sink fmt %d are different.",
			 source_format, sink_format);
		ret = -EINVAL;
		goto out;
	}

	switch (source_format) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		cd->data_shift = 0;
		cd->polyphase_func = src_polyphase_stage_cir_s16;
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		cd->data_shift = 8;
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		cd->data_shift = 0;
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(dev, "src_prepare(): invalid format %d", source_format);
		ret = -EINVAL;
		goto out;
	}

out:
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	buffer_release(sink_c);
	buffer_release(source_c);

	return ret;
}

static int src_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "src_reset()");

	cd->src_func = src_fallback;
	src_polyphase_reset(&cd->src);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_src = {
	.type = SOF_COMP_SRC,
	.uid = SOF_RT_UUID(src_uuid),
	.tctx = &src_tr,
	.ops = {
		.create = src_new,
		.free = src_free,
		.params = src_params,
		.cmd = src_cmd,
		.trigger = src_trigger,
		.copy = src_copy,
		.prepare = src_prepare,
		.reset = src_reset,
#if CONFIG_IPC_MAJOR_4
		.get_attribute = src_get_attribute,
#endif
	},
};

static SHARED_DATA struct comp_driver_info comp_src_info = {
	.drv = &comp_src,
};

UT_STATIC void sys_comp_src_init(void)
{
	comp_register(platform_shared_get(&comp_src_info,
					  sizeof(comp_src_info)));
}

DECLARE_MODULE(sys_comp_src_init);

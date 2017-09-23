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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
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

#define trace_volume(__e)	trace_event(TRACE_CLASS_VOLUME, __e)
#define tracev_volume(__e)	tracev_event(TRACE_CLASS_VOLUME, __e)
#define trace_volume_error(__e)	trace_error(TRACE_CLASS_VOLUME, __e)

/* this should ramp from 0dB to mute in 64ms.
 * i.e 2^16 -> 0 in 32 * 2048 steps each lasting 2ms
 */
#define VOL_RAMP_US	2000
#define VOL_RAMP_STEP	(1 << 11)
#define VOL_MAX		(1 << 16)

/*
 * Simple volume control
 *
 * Gain amplitude value is between 0 (mute) ... 2^16 (0dB) ... 2^24 (~+48dB)
 *
 * Currently we use 16 bit data for copies to/from DAIs and HOST PCM buffers,
 * 32 bit data is used in all other cases for overhead.
 * TODO: Add 24 bit (4 byte aligned) support using HiFi2 EP SIMD.
 */

/* volume component private data */
struct comp_data {
	uint32_t source_period_bytes;
	uint32_t sink_period_bytes;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	uint32_t chan[SOF_IPC_MAX_CHANNELS];
	uint32_t volume[SOF_IPC_MAX_CHANNELS];	/* current volume */
	uint32_t tvolume[SOF_IPC_MAX_CHANNELS];	/* target volume */
	uint32_t mvolume[SOF_IPC_MAX_CHANNELS];	/* mute volume */
	void (*scale_vol)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source, uint32_t frames);
	struct work volwork;

	/* host volume readback */
	struct sof_ipc_ctrl_value_chan *hvol;
};

struct comp_func_map {
	uint16_t source;	/* source format */
	uint16_t sink;		/* sink format */
	uint16_t channels;	/* channel number for the stream */
	void (*func)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source, uint32_t frames);
};

/* copy and scale volume from 16 bit source buffer to 32 bit dest buffer */
static void vol_s16_to_s32(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t*) source->r_ptr;
	int32_t i, *dest = (int32_t*) sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	for (i = 0; i < frames * 2; i += 2) {
		dest[i] = (int32_t)src[i] * cd->volume[0];
		dest[i + 1] = (int32_t)src[i + 1] * cd->volume[1];
	}
}

/* copy and scale volume from 32 bit source buffer to 16 bit dest buffer */
static void vol_s32_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t i, *src = (int32_t*) source->r_ptr;
	int16_t *dest = (int16_t*) sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	for (i = 0; i < frames * 2; i += 2) {
		dest[i] = (((int32_t)src[i] >> 16) * cd->volume[0]) >> 16;
		dest[i + 1] = (((int32_t)src[i + 1] >> 16) * cd->volume[1]) >> 16;
	}
}

/* copy and scale volume from 32 bit source buffer to 32 bit dest buffer */
static void vol_s32_to_s32(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t*) source->r_ptr;
	int32_t i, *dest = (int32_t*) sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	for (i = 0; i < frames * 2; i += 2) {
		dest[i] = ((int64_t)src[i] * cd->volume[0]) >> 16;
		dest[i + 1] = ((int64_t)src[i + 1] * cd->volume[1]) >> 16;
	}
}

/* copy and scale volume from 16 bit source buffer to 16 bit dest buffer */
static void vol_s16_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t*) source->r_ptr;
	int16_t *dest = (int16_t*) sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	for (i = 0; i < frames * 2; i += 2) {
		dest[i] = ((int32_t)src[i] * cd->volume[0]) >> 16;
		dest[i + 1] = ((int32_t)src[i + 1] * cd->volume[1]) >> 16;
	}
}

/* copy and scale volume from 16 bit source buffer to 24 bit on 32 bit boundary dest buffer */
static void vol_s16_to_s24(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t*) source->r_ptr;
	int32_t i, *dest = (int32_t*) sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	for (i = 0; i < frames * 2; i += 2) {
		dest[i] = ((int32_t)src[i] * cd->volume[0]) >> 8;
		dest[i + 1] = ((int32_t)src[i + 1] * cd->volume[1]) >> 8;
	}
}

/* copy and scale volume from 16 bit source buffer to 24 bit on 32 bit boundary dest buffer */
static void vol_s24_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t i, *src = (int32_t*) source->r_ptr;
	int16_t *dest = (int16_t*) sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	for (i = 0; i < frames * 2; i += 2) {
		dest[i] = (int16_t)((((int32_t)src[i] >> 8) *
			cd->volume[0]) >> 16);
		dest[i + 1] = (int16_t)((((int32_t)src[i + 1] >> 8) *
			cd->volume[1]) >> 16);
	}
}

/* copy and scale volume from 32 bit source buffer to 24 bit on 32 bit boundary dest buffer */
static void vol_s32_to_s24(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t*) source->r_ptr;
	int32_t i, *dest = (int32_t*) sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	for (i = 0; i < frames * 2; i += 2) {
		dest[i] = ((int64_t)src[i] * cd->volume[0]) >> 24;
		dest[i + 1] = ((int64_t)src[i + 1] * cd->volume[1]) >> 24;
	}
}

/* copy and scale volume from 16 bit source buffer to 24 bit on 32 bit boundary dest buffer */
static void vol_s24_to_s32(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t i, *src = (int32_t*) source->r_ptr;
	int32_t *dest = (int32_t*) sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	for (i = 0; i < frames * 2; i += 2) {
		dest[i] = (int32_t)(((int64_t)src[i]  *
			cd->volume[0]) >> 8);
		dest[i + 1] = (int32_t)(((int64_t)src[i + 1] *
			cd->volume[1]) >> 8);
	}
}

/* map of source and sink buffer formats to volume function */
static const struct comp_func_map func_map[] = {
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, 2, vol_s16_to_s16},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, 2, vol_s16_to_s32},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, 2, vol_s32_to_s16},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, 2, vol_s32_to_s32},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, 2, vol_s16_to_s24},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, 2, vol_s24_to_s16},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, 2, vol_s32_to_s24},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, 2, vol_s24_to_s32},
};

/* synchronise host mmap() volume with real value */
static void vol_sync_host(struct comp_data *cd, uint32_t chan)
{
	int i;

	if (cd->hvol == NULL)
		return;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++) {
		if (cd->hvol[i].channel == cd->chan[chan])
			cd->hvol[i].value = cd->volume[chan];
	}
}

static void vol_update(struct comp_data *cd, uint32_t chan)
{
	cd->volume[chan] = cd->tvolume[chan];
	vol_sync_host(cd, chan);
}

/* this ramps volume changes over time */
static uint32_t vol_work(void *data, uint32_t delay)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t vol;
	int i, again = 0;

	/* inc/dec each volume if it's not at target */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {

		/* skip if target reached */
		if (cd->volume[i] == cd->tvolume[i]) {
			continue;
		}

		vol = cd->volume[i];

		if (cd->volume[i] < cd->tvolume[i]) {
			/* ramp up */
			vol += VOL_RAMP_STEP;

			/* ramp completed ? */
			if (vol >= cd->tvolume[i] || vol >= VOL_MAX)
				vol_update(cd, i);
			else {
				cd->volume[i] = vol;
				again = 1;
			}
		} else {
			/* ramp down */
			vol -= VOL_RAMP_STEP;

			/* ramp completed ? */
			if (vol <= cd->tvolume[i] || vol >= VOL_MAX)
				vol_update(cd, i);
			else {
				cd->volume[i] = vol;
				again = 1;
			}
		}

		/* sync host with new value */
		vol_sync_host(cd, i);
	}

	/* do we need to continue ramping */
	if (again)
		return VOL_RAMP_US;
	else
		return 0;
}

static struct comp_dev *volume_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_volume *vol;
	struct sof_ipc_comp_volume *ipc_vol = (struct sof_ipc_comp_volume *)comp;
	struct comp_data *cd;
	int i;

	trace_volume("new");

	dev = rzalloc(RZONE_RUNTIME, RFLAGS_NONE,
		COMP_SIZE(struct sof_ipc_comp_volume));
	if (dev == NULL)
		return NULL;

	vol = (struct sof_ipc_comp_volume *)&dev->comp;
	memcpy(vol, ipc_vol, sizeof(struct sof_ipc_comp_volume));

	cd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*cd));
	if (cd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	work_init(&cd->volwork, vol_work, dev, WORK_ASYNC);

	/* set the default volumes */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		cd->volume[i] = VOL_MAX;
		cd->tvolume[i] = VOL_MAX;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

static void volume_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_volume("fre");

	rfree(cd);
	rfree(dev);
}

/*
 * Set volume component audio stream paramters - All done in prepare() since
 * wee need to know source and sink component params.
 */
static int volume_params(struct comp_dev *dev)
{
	trace_volume("par");

	return 0;
}

static inline void volume_set_chan(struct comp_dev *dev, int chan, uint32_t vol)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	/* TODO: ignore vol of 0 atm - bad IPC */
	if (vol > 0 && vol <= VOL_MAX)
		cd->tvolume[chan] = vol;
}

static inline void volume_set_chan_mute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->mvolume[chan] = cd->volume[chan];
	cd->tvolume[chan] = 0;
}

static inline void volume_set_chan_unmute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->tvolume[chan] = cd->mvolume[chan];
}

static int volume_ctrl_set_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i, j;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems >= SOF_IPC_MAX_CHANNELS) {
		trace_volume_error("gs0");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:

		for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++) {
			for (j = 0; j < cdata->num_elems; j++) {
				if (cdata->chanv[j].value == cd->chan[i])
					volume_set_chan(dev, i, cdata->chanv[j].value);
			}
		}

		work_schedule_default(&cd->volwork, VOL_RAMP_US);
		break;
	case SOF_CTRL_CMD_MUTE:

		for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++) {
			for (j = 0; j < cdata->num_elems; j++) {
				if (cdata->chanv[j].value == cd->chan[i])
					volume_set_chan_mute(dev, i);
			}
		}
		work_schedule_default(&cd->volwork, VOL_RAMP_US);
		break;
	case SOF_CTRL_CMD_UNMUTE:

		for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++) {
			for (j = 0; j < cdata->num_elems; j++) {
				if (cdata->chanv[j].value == cd->chan[i])
					volume_set_chan_unmute(dev, i);
			}
		}
		work_schedule_default(&cd->volwork, VOL_RAMP_US);
		break;
	default:
		trace_volume_error("gs1");
		return -EINVAL;
	}

	return 0;
}

static int volume_ctrl_get_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i, j;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems >= SOF_IPC_MAX_CHANNELS) {
		trace_volume_error("gc0");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:

		for (i = 0; i < cdata->num_elems; i++) {
			for (j = 0; j < cdata->num_elems; j++) {
				if (cdata->chanv[j].value == cd->chan[i])
					cdata->chanv[j].value = cd->tvolume[i];
			}
		}

		break;
	default:
		trace_volume_error("ec2");
		return -EINVAL;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int volume_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret;

	trace_volume("cmd");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		return volume_ctrl_set_cmd(dev, cdata);
	case COMP_CMD_GET_VALUE:
		return volume_ctrl_get_cmd(dev, cdata);
	default:
		return ret;
	}
}

/* copy and process stream data from source to sink buffers */
static int volume_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink, *source;
	uint32_t copy_bytes;

	tracev_volume("cpy");

	/* volume components will only ever have 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	/* get max number of bytes that can be copied */
	copy_bytes = comp_buffer_get_copy_bytes(dev, source, sink);

	/* Run volume if buffers have enough room */
	if (copy_bytes < cd->source_period_bytes) {
		comp_underrun(dev, source, copy_bytes, cd->source_period_bytes);
		return 0;
	}
	if (copy_bytes < cd->sink_period_bytes) {
		comp_overrun(dev, sink, copy_bytes, cd->sink_period_bytes);
		return 0;
	}

	/* copy and scale volume */
	cd->scale_vol(dev, sink, source, dev->frames);

	/* calc new free and available */
	comp_update_buffer_produce(sink, cd->sink_period_bytes);
	comp_update_buffer_consume(source, cd->source_period_bytes);

	return dev->frames;
}

/*
 * Volume componnet is usually first and last in pipelines so it makes sense
 * to also do some type conversion here too.
 */
static int volume_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb, *sourceb;
	struct sof_ipc_comp_config *sconfig, *config = COMP_GET_CONFIG(dev);
	int i, ret;

	trace_volume("pre");

	/* volume components will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	/* get source data format */
	switch (sourceb->source->comp.type) {
	case SOF_COMP_HOST:
	case SOF_COMP_SG_HOST:
		/* source format comes from IPC params */
		cd->source_format = sourceb->source->params.frame_fmt;
		cd->source_period_bytes = dev->frames *
			comp_frame_bytes(sourceb->source);
		break;
	case SOF_COMP_DAI:
	case SOF_COMP_SG_DAI:
	default:
		/* source format comes from DAI/comp config */
		sconfig = COMP_GET_CONFIG(sourceb->source);
		cd->source_format = sconfig->frame_fmt;
		cd->source_period_bytes = dev->frames *
			comp_frame_bytes(sourceb->source);
		break;
	}

	/* get sink data format */
	switch (sinkb->sink->comp.type) {
	case SOF_COMP_HOST:
	case SOF_COMP_SG_HOST:
		/* sink format come from IPC params */
		cd->sink_format = sinkb->sink->params.frame_fmt;
		cd->sink_period_bytes = dev->frames *
			comp_frame_bytes(sinkb->sink);
		break;
	case SOF_COMP_DAI:
	case SOF_COMP_SG_DAI:
	default:
		/* sink format comes from DAI/comp config */
		sconfig = COMP_GET_CONFIG(sinkb->sink);
		cd->sink_format = sconfig->frame_fmt;
		cd->sink_period_bytes = dev->frames *
			comp_frame_bytes(sinkb->sink);
		break;
	}

	dev->frame_bytes = cd->sink_period_bytes;

	/* set downstream buffer size */
	ret = buffer_set_size(sinkb, cd->sink_period_bytes *
		config->periods_sink);
	if (ret < 0) {
		trace_volume_error("vp0");
		return ret;
	}

	buffer_reset_pos(sinkb);

	/* validate */
	if (cd->sink_period_bytes == 0) {
		trace_volume_error("vp1");
		trace_value(dev->frames);
		trace_value(sinkb->sink->frame_bytes);
		return -EINVAL;
	}
	if (cd->source_period_bytes == 0) {
		trace_volume_error("vp2");
		trace_value(dev->frames);
		trace_value(sourceb->source->frame_bytes);
		return -EINVAL;
	}

	/* map the volume function for source and sink buffers */
	for (i = 0; i < ARRAY_SIZE(func_map); i++) {

		if (cd->source_format != func_map[i].source)
			continue;
		if (cd->sink_format != func_map[i].sink)
			continue;
		if (dev->params.channels != func_map[i].channels)
			continue;

		cd->scale_vol = func_map[i].func;
		goto found;
	}
	trace_volume_error("vp3");
	trace_value(cd->source_format);
	trace_value(cd->sink_format);
	trace_value(dev->params.channels);
	return -EINVAL;

found:
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		vol_sync_host(cd, i);

	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static int volume_preload(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;

	trace_volume("PrL");

	/* make sure there is enough space in sink buffer */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);
	if (sink->free < cd->sink_period_bytes)
		return 0;

	return volume_copy(dev);
}

static int volume_reset(struct comp_dev *dev)
{
	trace_volume("res");

	dev->state = COMP_STATE_READY;
	return 0;
}

struct comp_driver comp_volume = {
	.type	= SOF_COMP_VOLUME,
	.ops	= {
		.new		= volume_new,
		.free		= volume_free,
		.params		= volume_params,
		.cmd		= volume_cmd,
		.copy		= volume_copy,
		.prepare	= volume_prepare,
		.reset		= volume_reset,
		.preload	= volume_preload,
	},
};

void sys_comp_volume_init(void)
{
	comp_register(&comp_volume);
}

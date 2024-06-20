// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016-2024 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/init.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "copier/copier.h"
#include "copier/dai_copier.h"

#include <zephyr/device.h>
#include <zephyr/drivers/dai.h>

#define FAKE_DAI_BEGIN_IGNORE_GLITCHES_COUNT	100	/* Only warn # periods in start */
#define FAKE_DAI_PRODUCE_GLITCHES		0	/* Force glitches to waveform */
#define FAKE_DAI_PRODUCE_GLITCH_RATE		5000	/* Glitch every # periods */

#define FAKE_DAI_MAX_BYTES_FREE_AVAIL	1152 /* emprical value seen in running system */

static const struct comp_driver comp_dai;

LOG_MODULE_REGISTER(dai_comp, CONFIG_SOF_LOG_LEVEL);

/* c2b00d27-ffbc-4150-a51a-245c79c5e54b */
DECLARE_SOF_RT_UUID("dai", dai_comp_uuid, 0xc2b00d27, 0xffbc, 0x4150,
		    0xa5, 0x1a, 0x24, 0x5c, 0x79, 0xc5, 0xe5, 0x4b);

DECLARE_TR_CTX(dai_comp_tr, SOF_UUID(dai_comp_uuid), LOG_LEVEL_INFO);


/*
 * Fake DAI versions of library functions
 */

static void fake_dai_dma_position_update(struct dai_data *dd, struct comp_dev *dev)
{
}

static int fake_dai_trigger(const struct device *dev, int direction, int cmd)
{
	return 0;
}

#define FAKE_GET_DEVICE_LIST(node) DEVICE_DT_GET(node),

const struct device *fake_zephyr_dev[] = {
#if CONFIG_DAI_INTEL_SSP
	DT_FOREACH_STATUS_OKAY(intel_ssp_dai, FAKE_GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_INTEL_DMIC
	DT_FOREACH_STATUS_OKAY(intel_dai_dmic, FAKE_GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_INTEL_ALH
	DT_FOREACH_STATUS_OKAY(intel_alh_dai, FAKE_GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_INTEL_HDA
	DT_FOREACH_STATUS_OKAY(intel_hda_dai, FAKE_GET_DEVICE_LIST)
#endif
};

static const struct device *fake_dai_get_zephyr_device(uint32_t type, uint32_t index)
{
	struct dai_config cfg;
	int dir;
	int i;

	dir = (type == SOF_DAI_INTEL_DMIC) ? DAI_DIR_RX : DAI_DIR_BOTH;

	for (i = 0; i < ARRAY_SIZE(fake_zephyr_dev); i++) {
		if (dai_config_get(fake_zephyr_dev[i], &cfg, dir))
			continue;
		if (cfg.type == type && cfg.dai_index == index)
			return fake_zephyr_dev[i];
	}

	return NULL;
}

static struct dai *fake_dai_get(uint32_t type, uint32_t index, uint32_t flags)
{
	const struct device *dev;
	struct dai *d;

	dev = fake_dai_get_zephyr_device(type, index);
	if (!dev) {
		tr_err(&dai_tr, "dai_get: failed to get dai with index %d type %d",
		       index, type);
		return NULL;
	}

	d = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(struct dai));
	if (!d)
		return NULL;

	d->index = index;
	d->type = type;
	d->dev = dev;

	// dai_set_device_params(d);

	return d;
}

/*
 * Next is copy & remove of dai-zephyr.c
 */

#if CONFIG_COMP_DAI_GROUP

static int dai_comp_trigger_internal(struct dai_data *dd, struct comp_dev *dev, int cmd);

static void dai_atomic_trigger(void *arg, enum notify_id type, void *data)
{
	struct comp_dev *dev = arg;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_group *group = dd->group;

	/* Atomic context set by the last DAI to receive trigger command */
	group->trigger_ret = dai_comp_trigger_internal(dd, dev, group->trigger_cmd);
}

/* Assign DAI to a group */
int dai_assign_group(struct dai_data *dd, struct comp_dev *dev, uint32_t group_id)
{
	if (dd->group) {
		if (dd->group->group_id != group_id) {
			comp_err(dev, "dai_assign_group(), DAI already in group %d, requested %d",
				 dd->group->group_id, group_id);
			return -EINVAL;
		}

		/* No need to re-assign to the same group, do nothing */
		return 0;
	}

	dd->group = dai_group_get(group_id, DAI_CREAT);
	if (!dd->group) {
		comp_err(dev, "dai_assign_group(), failed to assign group %d",
			 group_id);
		return -EINVAL;
	}

	comp_dbg(dev, "dai_assign_group(), group %d num %d",
		 group_id, dd->group->num_dais);

	/* Register for the atomic trigger event */
	notifier_register(dev, dd->group, NOTIFIER_ID_DAI_TRIGGER,
			  dai_atomic_trigger, 0);

	return 0;
}
#endif

static int dai_trigger_op(struct dai *dai, int cmd, int direction)
{
	const struct device *dev = dai->dev;
	enum dai_trigger_cmd zephyr_cmd;

	switch (cmd) {
	case COMP_TRIGGER_STOP:
		zephyr_cmd = DAI_TRIGGER_STOP;
		break;
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		zephyr_cmd = DAI_TRIGGER_START;
		break;
	case COMP_TRIGGER_PAUSE:
		zephyr_cmd = DAI_TRIGGER_PAUSE;
		break;
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_PRE_RELEASE:
		zephyr_cmd = DAI_TRIGGER_PRE_START;
		break;
	default:
		return -EINVAL;
	}

	return fake_dai_trigger(dev, direction, zephyr_cmd);
}

/* called from src/ipc/ipc3/handler.c and src/ipc/ipc4/dai.c */
int dai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
		   const void *spec_config)
{
	return 0;
}

/* called from ipc/ipc3/dai.c */
int dai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return 0;
}

/* called from ipc/ipc3/dai.c and ipc/ipc4/dai.c */
int dai_get_fifo_depth(struct dai *dai, int direction)
{
	const struct dai_properties *props;
	k_spinlock_key_t key;
	int fifo_depth;

	if (!dai)
		return 0;

	key = k_spin_lock(&dai->lock);
	props = dai_get_properties(dai->dev, direction, 0);
	fifo_depth = props->fifo_depth;
	k_spin_unlock(&dai->lock, key);

	return fifo_depth;
}

int dai_get_stream_id(struct dai *dai, int direction)
{
	k_spinlock_key_t key = k_spin_lock(&dai->lock);
	const struct dai_properties *props = dai_get_properties(dai->dev, direction, 0);
	int stream_id = props->stream_id;

	k_spin_unlock(&dai->lock, key);

	return stream_id;
}

static void dai_init_glitch_check(struct dai_triangle_generator_state *tg,
				  struct dai_glitch_detect_state *gd)
{
	int i;

	/* Start PCM code for channels, 1, -1, 1001, -1001, ...
	 * Channels 0, 2, ... count up
	 * Channels 1, 3, ... count down
	 * Count direction becomes opposite when max PCM code value is reached
	 */
	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++) {
		if ((i & 1) == 0)
			tg->pcm_increment[i] = 1;
		else
			tg->pcm_increment[i] = -1;

		gd->prev_pcm_value[i] = 0;
		gd->zeros_count[i] = 0;
		gd->no_signal[i] = true;
		tg->prev_pcm_value[i] = (i >> 1) * 1000 + tg->pcm_increment[i];
	}

	gd->first_value = true;
	gd->zeros_count_reported = false;
	gd->ignore_count = FAKE_DAI_BEGIN_IGNORE_GLITCHES_COUNT;
	tg->countdown = FAKE_DAI_PRODUCE_GLITCH_RATE;
	tg->first_copy = true;
}

static void dai_glitch_core(struct comp_dev *dev,
			    struct dai_glitch_detect_state *gd,
			    int *c,
			    bool *glitch,
			    int channels,
			    int32_t value)
{
	int32_t delta;

	delta = gd->prev_pcm_value[*c] - value;
	if (!gd->first_value && ABS(delta) > 1) {
		comp_dbg(dev, "dai_detect_glitch(), current %d, previous %d",
			 value, gd->prev_pcm_value[*c]);
		gd->glitch_count[*c]++;
		*glitch = true;
	}

	if (value == 0 && gd->no_signal[*c])
		gd->zeros_count[*c]++;
	else
		gd->no_signal[*c] = false;

	gd->prev_pcm_value[*c] = value;
	*c += 1;
	if (*c == channels) {
		gd->first_value = false;
		*c = 0;
	}
}

static int dai_detect_glitch(struct comp_dev *dev,
			     struct dai_glitch_detect_state *gd,
			     struct comp_buffer *source_buffer,
			     struct comp_buffer *sink_buffer,
			     uint32_t source_bytes)
{
	struct audio_stream *source = &source_buffer->stream;
	int32_t *x32;
	int16_t *x16;
	int channels;
	int frames;
	int samples;
	enum sof_ipc_frame source_format;
	int i, n, nmax;
	int processed = 0;
	int c = 0;
	bool glitch = false;
	bool no_signal_yet = false;

	source_format = audio_stream_get_frm_fmt(source);
	switch (source_format) {
	case SOF_IPC_FRAME_S16_LE:
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		break;
	default:
		return -EINVAL;
	}

	frames = source_bytes / audio_stream_frame_bytes(source);
	channels = audio_stream_get_channels(source);
	samples = frames * channels;

	comp_dbg(dev, "dai_detect_glitch() frames = %d, channels = %d, source_bytes = %d",
		 frames, channels, source_bytes);

	if (source_format == SOF_IPC_FRAME_S16_LE) {
		x16 = audio_stream_get_rptr(source);
		while (processed < samples) {
			nmax = samples - processed;
			n = audio_stream_bytes_without_wrap(source, x16) >> 1;
			n = MIN(n, nmax);
			for (i = 0; i < n; i++) {
				dai_glitch_core(dev, gd, &c, &glitch, channels, (int32_t)(*x16));
				x16++;
			}
			x16 = audio_stream_wrap(source, x16);
			processed += n;
		}
	} else {
		x32 = audio_stream_get_rptr(source);
		while (processed < samples) {
			nmax = samples - processed;
			n = audio_stream_bytes_without_wrap(source, x32) >> 2;
			n = MIN(n, nmax);
			for (i = 0; i < n; i++) {
				dai_glitch_core(dev, gd, &c, &glitch, channels, *x32);
				x32++;
			}
			x32 = audio_stream_wrap(source, x32);
			processed += n;
		}
	}

	comp_update_buffer_consume(source_buffer, source_bytes);

	if (gd->ignore_count > 0)
		gd->ignore_count--;

	if (glitch) {
		if (gd->ignore_count)
			comp_warn(dev, "dai_detect_glitch(): Glitches count for channels %d %d %d %d",
				  gd->glitch_count[0], gd->glitch_count[1],
				  gd->glitch_count[2], gd->glitch_count[3]);
		else
			comp_err(dev, "dai_detect_glitch(): Glitches count for channels %d %d %d %d",
				 gd->glitch_count[0], gd->glitch_count[1],
				 gd->glitch_count[2], gd->glitch_count[3]);
	}

	for (i = 0; i < channels; i++)
		if (gd->no_signal[i])
			no_signal_yet = true;

	if (!no_signal_yet && !gd->zeros_count_reported) {
		comp_info(dev, "dai_detect_glitch(): Zero PCM samples count for channels %d %d %d %d",
			  gd->zeros_count[0], gd->zeros_count[1],
			  gd->zeros_count[2], gd->zeros_count[3]);
		gd->zeros_count_reported = true;
	}

	return 0;
}

static int32_t dai_triangle_core(struct dai_triangle_generator_state *tg,
				 int *c, int channels, int32_t max_val)
{
	int32_t next;

	next = tg->prev_pcm_value[*c] + tg->pcm_increment[*c];
	tg->prev_pcm_value[*c] = next;
	if (next == max_val || next == -max_val)
		tg->pcm_increment[*c] = -tg->pcm_increment[*c];

#if FAKE_DAI_PRODUCE_GLITCHES
	if (!tg->countdown)
		next = 0;
#endif
	*c += 1;
	if (*c == channels)
		*c = 0;

	return next;
}

static int dai_produce_triangle(struct comp_dev *dev,
				struct dai_triangle_generator_state *tg,
				struct comp_buffer *source_buffer,
				struct comp_buffer *sink_buffer,
				uint32_t source_bytes)
{
	struct audio_stream *source = &source_buffer->stream;
	struct audio_stream *sink = &sink_buffer->stream;
	int32_t max_val;
	int32_t *y32;
	int16_t *y16;
	int sink_bytes;
	int channels;
	int frames;
	int samples;
	enum sof_ipc_frame sink_format;
	int i, n, nmax;
	int processed = 0;
	int c = 0;

	sink_format = audio_stream_get_frm_fmt(sink);
	switch (sink_format) {
	case SOF_IPC_FRAME_S16_LE:
		max_val = INT16_MAX;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		max_val = INT24_MAXVALUE;
		break;
	case SOF_IPC_FRAME_S32_LE:
		max_val = INT32_MAX;
		break;
	default:
		return -EINVAL;
	}

	frames = source_bytes / audio_stream_frame_bytes(source);
	channels = audio_stream_get_channels(sink);
	samples = frames * channels;
	sink_bytes = audio_stream_sample_bytes(sink) * samples;

	comp_dbg(dev, "dai_produce_triangle() frames = %d, channels = %d, sink_bytes = %d",
		 frames, channels, sink_bytes);

#if FAKE_DAI_PRODUCE_GLITCHES
	tg->countdown--;
#endif
	if (sink_format == SOF_IPC_FRAME_S16_LE) {
		y16 = audio_stream_get_wptr(sink);
		while (processed < samples) {
			nmax = samples - processed;
			n = audio_stream_bytes_without_wrap(sink, y16) >> 1;
			n = MIN(n, nmax);
			for (i = 0; i < n; i++) {
				*y16 = (int16_t)dai_triangle_core(tg, &c, channels, max_val);
				y16++;
			}
			y16 = audio_stream_wrap(sink, y16);
			processed += n;
		}
	} else {
		y32 = audio_stream_get_wptr(sink);
		while (processed < samples) {
			nmax = samples - processed;
			n = audio_stream_bytes_without_wrap(sink, y32) >> 2;
			n = MIN(n, nmax);
			for (i = 0; i < n; i++) {
				*y32 = dai_triangle_core(tg, &c, channels, max_val);
				y32++;
			}
			y32 = audio_stream_wrap(sink, y32);
			processed += n;
		}
	}

#if FAKE_DAI_PRODUCE_GLITCHES
	if (!tg->countdown)
		tg->countdown = FAKE_DAI_PRODUCE_GLITCH_RATE;
#endif

	buffer_stream_writeback(sink_buffer, sink_bytes);
	comp_update_buffer_produce(sink_buffer, sink_bytes);
	return 0;
}

/* this is called by DMA driver every time descriptor has completed */
static enum dma_cb_status
dai_dma_cb(struct dai_data *dd, struct comp_dev *dev, uint32_t bytes,
	   pcm_converter_func *converter)
{
	enum dma_cb_status dma_status = DMA_CB_STATUS_RELOAD;
	int ret = 0;

	comp_dbg(dev, "dai_dma_cb()");

	/* stop dma copy for pause/stop/xrun */
	if (dev->state != COMP_STATE_ACTIVE || dd->xrun) {
		/* stop the DAI */
		dai_trigger_op(dd->dai, COMP_TRIGGER_STOP, dev->direction);

		/* tell DMA not to reload */
		dma_status = DMA_CB_STATUS_END;
	}

	/* is our pipeline handling an XRUN ? */
	if (dd->xrun) {
		/* make sure we only playback silence during an XRUN */
		if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
			/* fill buffer with silence */
			buffer_zero(dd->dma_buffer);

		return dma_status;
	}

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		ret = dai_detect_glitch(dev, &dd->glitch,
					dd->local_buffer, dd->dma_buffer, bytes);
	} else {
		audio_stream_invalidate(&dd->dma_buffer->stream, bytes);

		if (dd->triangle.first_copy) {
			comp_info(dev, "dai_dma_cb(): First DAI copy for triangle wave");
			dd->triangle.first_copy = false;
		}

		ret = dai_produce_triangle(dev, &dd->triangle,
					   dd->dma_buffer, dd->local_buffer, bytes);

#if CONFIG_IPC_MAJOR_4
		struct list_item *sink_list;
		/* Skip in case of endpoint DAI devices created by the copier */
		if (converter) {
			/*
			 * copy from DMA buffer to all sink buffers using the right PCM converter
			 * function
			 */
			list_for_item(sink_list, &dev->bsink_list) {
				struct comp_dev *sink_dev;
				struct comp_buffer *sink;
				int j;

				sink = container_of(sink_list, struct comp_buffer, source_list);

				/* this has been handled above already */
				if (sink == dd->local_buffer)
					continue;

				sink_dev = sink->sink;

				j = IPC4_SINK_QUEUE_ID(buf_get_id(sink));

				if (j >= IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT) {
					comp_err(dev, "Sink queue ID: %d >= max output pin count: %d\n",
						 j, IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT);
					ret = -EINVAL;
					continue;
				}

				if (!converter[j]) {
					comp_err(dev, "No PCM converter for sink queue %d\n", j);
					ret = -EINVAL;
					continue;
				}
			}
		}
#endif
		audio_stream_consume(&dd->dma_buffer->stream, bytes);
	}

	/* assert dma_buffer_copy succeed */
	if (ret < 0) {
		struct comp_buffer *source_c, *sink_c;

		source_c = dev->direction == SOF_IPC_STREAM_PLAYBACK ?
					dd->local_buffer : dd->dma_buffer;
		sink_c = dev->direction == SOF_IPC_STREAM_PLAYBACK ?
					dd->dma_buffer : dd->local_buffer;
		comp_err(dev, "dai_dma_cb() dma buffer copy failed, dir %d bytes %d avail %d free %d",
			 dev->direction, bytes,
			 audio_stream_get_avail_samples(&source_c->stream) *
				audio_stream_frame_bytes(&source_c->stream),
			 audio_stream_get_free_samples(&sink_c->stream) *
				audio_stream_frame_bytes(&sink_c->stream));
	} else {
		/* update host position (in bytes offset) for drivers */
		dd->total_data_processed += bytes;
	}

	return dma_status;
}

/* this is called by DMA driver every time descriptor has completed */
static enum dma_cb_status
dai_dma_multi_endpoint_cb(struct dai_data *dd, struct comp_dev *dev, uint32_t frames,
			  struct comp_buffer *multi_endpoint_buffer)
{
	enum dma_cb_status dma_status = DMA_CB_STATUS_RELOAD;
	uint32_t i, bytes;

	comp_dbg(dev, "dai_dma_multi_endpoint_cb()");

	/* stop dma copy for pause/stop/xrun */
	if (dev->state != COMP_STATE_ACTIVE || dd->xrun) {
		/* stop the DAI */
		dai_trigger_op(dd->dai, COMP_TRIGGER_STOP, dev->direction);

		/* tell DMA not to reload */
		dma_status = DMA_CB_STATUS_END;
	}

	/* is our pipeline handling an XRUN ? */
	if (dd->xrun) {
		/* make sure we only playback silence during an XRUN */
		if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
			/* fill buffer with silence */
			buffer_zero(dd->dma_buffer);

		return dma_status;
	}

	bytes = frames * audio_stream_frame_bytes(&dd->dma_buffer->stream);
	if (dev->direction == SOF_IPC_STREAM_CAPTURE)
		audio_stream_invalidate(&dd->dma_buffer->stream, bytes);

	/* copy all channels one by one */
	for (i = 0; i < audio_stream_get_channels(&dd->dma_buffer->stream); i++) {
		uint32_t multi_buf_channel = dd->dma_buffer->chmap[i];

		if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
			dd->process(&multi_endpoint_buffer->stream, multi_buf_channel,
				    &dd->dma_buffer->stream, i, frames);
		else
			dd->process(&dd->dma_buffer->stream, i, &multi_endpoint_buffer->stream,
				    multi_buf_channel, frames);
	}

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		audio_stream_writeback(&dd->dma_buffer->stream, bytes);
		audio_stream_produce(&dd->dma_buffer->stream, bytes);
	} else {
		audio_stream_consume(&dd->dma_buffer->stream, bytes);
	}

	/* update host position (in bytes offset) for drivers */
	dd->total_data_processed += bytes;

	return dma_status;
}

int dai_common_new(struct dai_data *dd, struct comp_dev *dev,
		   const struct ipc_config_dai *dai_cfg)
{
	uint32_t dir;

	dd->dai = fake_dai_get(dai_cfg->type, dai_cfg->dai_index, DAI_CREAT);
	if (!dd->dai) {
		comp_err(dev, "dai_new(): dai_get() failed to create DAI.");
		return -ENODEV;
	}

	dd->ipc_config = *dai_cfg;

	/* request GP LP DMA with shared access privilege */
	dir = dai_cfg->direction == SOF_IPC_STREAM_PLAYBACK ?
		DMA_DIR_MEM_TO_DEV : DMA_DIR_DEV_TO_MEM;

	k_spinlock_init(&dd->dai->lock);

	dd->xrun = 0;
	dd->chan = NULL;

	return 0;
}

static struct comp_dev *dai_new(const struct comp_driver *drv,
				const struct comp_ipc_config *config,
				const void *spec)
{
	struct comp_dev *dev;
	const struct ipc_config_dai *dai_cfg = spec;
	struct dai_data *dd;
	int ret;

	comp_cl_warn(&comp_dai, "dai_new(): Simulated DAI, no real playback or capture happens");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	dd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dd));
	if (!dd)
		goto e_data;

	comp_set_drvdata(dev, dd);

	ret = dai_common_new(dd, dev, dai_cfg);
	if (ret < 0)
		goto error;

	dev->state = COMP_STATE_READY;

	return dev;

error:
	rfree(dd);
e_data:
	rfree(dev);
	return NULL;
}

void dai_common_free(struct dai_data *dd)
{
	if (dd->group)
		dai_group_put(dd->group);

	if (dd->chan) {
		dma_release_channel(dd->dma->z_dev, dd->chan->index);
		dd->chan->dev_data = NULL;
	}

	dai_release_llp_slot(dd);

	dai_put(dd->dai);

	rfree(dd->dai_spec_config);
}

static void dai_free(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dd->group)
		notifier_unregister(dev, dd->group, NOTIFIER_ID_DAI_TRIGGER);

	dai_common_free(dd);

	rfree(dd);
	rfree(dev);
}

int dai_common_get_hw_params(struct dai_data *dd, struct comp_dev *dev,
			     struct sof_ipc_stream_params *params, int dir)
{
	struct dai_config cfg;
	int ret;

	comp_dbg(dev, "dai_common_get_hw_params()");

	ret = dai_config_get(dd->dai->dev, &cfg, dir);
	if (ret)
		return ret;

	params->rate = cfg.rate;
	params->buffer_fmt = 0;
	params->channels = cfg.channels;

	/* dai_comp_get_hw_params() function fetches hardware dai parameters,
	 * which then are propagating back through the pipeline, so that any
	 * component can convert specific stream parameter. Here, we overwrite
	 * frame_fmt hardware parameter as DAI component is able to convert
	 * stream with different frame_fmt's (using pcm converter)
	 */
	params->frame_fmt = dev->ipc_config.frame_fmt;

	return ret;
}

static int dai_comp_get_hw_params(struct comp_dev *dev,
				  struct sof_ipc_stream_params *params,
				  int dir)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	return dai_common_get_hw_params(dd, dev, params, dir);
}

static int dai_verify_params(struct dai_data *dd, struct comp_dev *dev,
			     struct sof_ipc_stream_params *params)
{
	struct sof_ipc_stream_params hw_params;
	int ret;

	memset(&hw_params, 0, sizeof(hw_params));

	ret = dai_common_get_hw_params(dd, dev, &hw_params, params->direction);
	if (ret < 0) {
		comp_err(dev, "dai_verify_params(): dai_verify_params failed ret %d", ret);
		return ret;
	}

	/* checks whether pcm parameters match hardware DAI parameter set
	 * during dai_set_config(). If hardware parameter is equal to 0, it
	 * means that it can vary, so any value is acceptable. We do not check
	 * format parameter, because DAI is able to change format using
	 * pcm_converter functions.
	 */
	if (hw_params.rate && hw_params.rate != params->rate) {
		comp_err(dev, "dai_verify_params(): pcm rate parameter %d does not match hardware rate %d",
			 params->rate, hw_params.rate);
		return -EINVAL;
	}

	if (hw_params.channels && hw_params.channels != params->channels) {
		comp_err(dev, "dai_verify_params(): pcm channels parameter %d does not match hardware channels %d",
			 params->channels, hw_params.channels);
		return -EINVAL;
	}

	/* set component period frames */
	component_set_nearest_period_frames(dev, params->rate);

	return 0;
}

static int dai_set_dma_buffer(struct dai_data *dd, struct comp_dev *dev,
			      struct sof_ipc_stream_params *params, uint32_t *pb, uint32_t *pc)
{
	struct sof_ipc_stream_params hw_params = *params;
	uint32_t frame_size;
	uint32_t period_count;
	uint32_t period_bytes;
	uint32_t buffer_size;
	uint32_t addr_align;
	uint32_t align;
	int err;

	comp_dbg(dev, "dai_set_dma_buffer()");

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		dd->local_buffer = list_first_item(&dev->bsource_list, struct comp_buffer,
						   sink_list);
	else
		dd->local_buffer = list_first_item(&dev->bsink_list, struct comp_buffer,
						   source_list);

	/* check if already configured */
	if (dev->state == COMP_STATE_PREPARE) {
		comp_info(dev, "dai_set_dma_buffer() component has been already configured.");
		return 0;
	}

	/* can set params on only init state */
	if (dev->state != COMP_STATE_READY) {
		comp_err(dev, "dai_set_dma_buffer(): comp state %d, expected COMP_STATE_READY.",
			 dev->state);
		return -EINVAL;
	}

	addr_align = sizeof(int32_t);
	align = sizeof(int32_t);

	/* calculate frame size */
	frame_size = get_frame_bytes(dev->ipc_config.frame_fmt, params->channels);

	/* calculate period size */
	period_bytes = dev->frames * frame_size;
	if (!period_bytes) {
		comp_err(dev, "dai_set_dma_buffer(): invalid period_bytes.");
		return -EINVAL;
	}

	dd->period_bytes = period_bytes;
	*pb = period_bytes;

	/* calculate DMA buffer size */
	period_count = SOF_DIV_ROUND_UP(dd->ipc_config.dma_buffer_size, period_bytes);
	buffer_size = ALIGN_UP(period_count * period_bytes, align);
	*pc = period_count;

	/* alloc DMA buffer or change its size if exists */
	if (dd->dma_buffer) {
		err = buffer_set_size(dd->dma_buffer, buffer_size, addr_align);

		if (err < 0) {
			comp_err(dev, "dai_set_dma_buffer(): buffer_size = %u failed", buffer_size);
			return err;
		}
	} else {
		dd->dma_buffer = buffer_alloc(buffer_size, SOF_MEM_CAPS_DMA, 0,
					      addr_align, false);
		if (!dd->dma_buffer) {
			comp_err(dev, "dai_set_dma_buffer(): failed to alloc dma buffer");
			return -ENOMEM;
		}

		/*
		 * dma_buffer should reffer to hardware dai parameters.
		 * Here, we overwrite frame_fmt hardware parameter as DAI
		 * component is able to convert stream with different
		 * frame_fmt's (using pcm converter).
		 */
		hw_params.frame_fmt = dev->ipc_config.frame_fmt;
		buffer_set_params(dd->dma_buffer, &hw_params, BUFFER_UPDATE_FORCE);
		dd->sampling = get_sample_bytes(hw_params.frame_fmt);
	}

	dd->fast_mode = dd->ipc_config.feature_mask & BIT(IPC4_COPIER_FAST_MODE);
	return 0;
}

int dai_common_params(struct dai_data *dd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	struct dma_sg_config *config = &dd->config;
	uint32_t period_bytes = 0;
	uint32_t period_count = 0;
	int err = 0;

	comp_dbg(dev, "dai_zephyr_params()");

	/* configure dai_data first */
	err = ipc_dai_data_config(dd, dev);
	if (err < 0) {
		comp_err(dev, "dai_zephyr_params(): ipc dai data config failed.");
		return err;
	}

	err = dai_verify_params(dd, dev, params);
	if (err < 0) {
		comp_err(dev, "dai_zephyr_params(): pcm params verification failed.");
		return -EINVAL;
	}

	err = dai_set_dma_buffer(dd, dev, params, &period_bytes, &period_count);
	if (err < 0) {
		comp_err(dev, "dai_zephyr_params(): alloc dma buffer failed.");
		goto out;
	}

out:
	/*
	 * Make sure to free all allocated items, all functions
	 * can be called with null pointer.
	 */
	if (err < 0) {
		buffer_free(dd->dma_buffer);
		dd->dma_buffer = NULL;
		dma_sg_free(&config->elem_array);
		rfree(dd->z_config);
		dd->z_config = NULL;
	}

	return err;
}

static int dai_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	comp_dbg(dev, "dai_params()");

	return dai_common_params(dd, dev, params);
}

int dai_common_config_prepare(struct dai_data *dd, struct comp_dev *dev)
{
	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_common_config_prepare(): Component is in active state.");
		return 0;
	}

	if (!dd->dai_spec_config) {
		comp_err(dev, "dai specific config is not set yet!");
		return -EINVAL;
	}

	if (dd->chan) {
		comp_info(dev, "dai_common_config_prepare(): dma channel index %d already configured",
			  dd->chan->index);
		return 0;
	}

	dai_init_glitch_check(&dd->triangle, &dd->glitch);
	return 0;
}

int dai_common_prepare(struct dai_data *dd, struct comp_dev *dev)
{

	dd->total_data_processed = 0;

	/* clear dma buffer to avoid pop noise */
	buffer_zero(dd->dma_buffer);

	/* dma reconfig not required if XRUN handling */
	if (dd->xrun) {
		/* after prepare, we have recovered from xrun */
		dd->xrun = 0;
		return 0;
	}

	return 0;
}

static int dai_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret;

	comp_warn(dev, "dai_prepare(): Simulated DAI, no real playback or capture happens");

	ret = dai_common_config_prepare(dd, dev);
	if (ret < 0)
		return ret;

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return dai_common_prepare(dd, dev);
}

void dai_common_reset(struct dai_data *dd, struct comp_dev *dev)
{
	struct dma_sg_config *config = &dd->config;

	/*
	 * DMA channel release should be skipped now for DAI's that support the two-step stop
	 * option. It will be done when the host sends the DAI_CONFIG IPC during hw_free.
	 */
	if (!dd->delayed_dma_stop)
		dai_dma_release(dd, dev);

	dma_sg_free(&config->elem_array);
	if (dd->z_config) {
		rfree(dd->z_config->head_block);
		rfree(dd->z_config);
		dd->z_config = NULL;
	}

	if (dd->dma_buffer) {
		buffer_free(dd->dma_buffer);
		dd->dma_buffer = NULL;
	}

	dd->wallclock = 0;
	dd->total_data_processed = 0;
	dd->xrun = 0;
}

static int dai_reset(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	comp_dbg(dev, "dai_reset()");

	dai_common_reset(dd, dev);

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

/* used to pass standard and bespoke command (with data) to component */
static int dai_comp_trigger_internal(struct dai_data *dd, struct comp_dev *dev, int cmd)
{
	int ret = 0;

	comp_dbg(dev, "dai_comp_trigger_internal(), command = %u", cmd);

	switch (cmd) {
	case COMP_TRIGGER_START:
		comp_dbg(dev, "dai_comp_trigger_internal(), START");

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0) {
			/* start the DAI */
			dai_trigger_op(dd->dai, cmd, dev->direction);
		} else {
			dd->xrun = 0;
		}

		platform_dai_wallclock(dev, &dd->wallclock);
		break;
	case COMP_TRIGGER_RELEASE:
		/* before release, we clear the buffer data to 0s,
		 * then there is no history data sent out after release.
		 * this is only supported at capture mode.
		 */
		if (dev->direction == SOF_IPC_STREAM_CAPTURE) {
			buffer_zero(dd->dma_buffer);
		}

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0)
			/* start the DAI */
			dai_trigger_op(dd->dai, cmd, dev->direction);
		else
			dd->xrun = 0;

		platform_dai_wallclock(dev, &dd->wallclock);
		break;
	case COMP_TRIGGER_XRUN:
		comp_info(dev, "dai_comp_trigger_internal(), XRUN");
		dd->xrun = 1;

		COMPILER_FALLTHROUGH;
	case COMP_TRIGGER_STOP:
		comp_dbg(dev, "dai_comp_trigger_internal(), STOP");
/*
 * Some platforms cannot just simple disable
 * DMA channel during the transfer,
 * because it will hang the whole DMA controller.
 * Therefore, stop the DMA first and let the DAI
 * drain the FIFO in order to stop the channel
 * as soon as possible.
 */
#if CONFIG_COMP_DAI_TRIGGER_ORDER_REVERSE
		dai_trigger_op(dd->dai, cmd, dev->direction);
#else
		dai_trigger_op(dd->dai, cmd, dev->direction);
#endif
		break;
	case COMP_TRIGGER_PAUSE:
		comp_dbg(dev, "dai_comp_trigger_internal(), PAUSE");
#if CONFIG_COMP_DAI_TRIGGER_ORDER_REVERSE
		dai_trigger_op(dd->dai, cmd, dev->direction);
#else
		dai_trigger_op(dd->dai, cmd, dev->direction);
#endif
		break;
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_PRE_RELEASE:
		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun)
			dd->xrun = 0;
		else
			dai_trigger_op(dd->dai, cmd, dev->direction);
		break;
	}

	return ret;
}

int dai_common_trigger(struct dai_data *dd, struct comp_dev *dev, int cmd)
{
	struct dai_group *group = dd->group;
	uint32_t irq_flags;
	int ret = 0;

	/* DAI not in a group, use normal trigger */
	if (!group) {
		comp_dbg(dev, "dai_common_trigger(), non-atomic trigger");
		return dai_comp_trigger_internal(dd, dev, cmd);
	}

	/* DAI is grouped, so only trigger when the entire group is ready */

	if (!group->trigger_counter) {
		/* First DAI to receive the trigger command,
		 * prepare for atomic trigger
		 */
		comp_dbg(dev, "dai_common_trigger(), begin atomic trigger for group %d",
			 group->group_id);
		group->trigger_cmd = cmd;
		group->trigger_counter = group->num_dais - 1;
	} else if (group->trigger_cmd != cmd) {
		/* Already processing a different trigger command */
		comp_err(dev, "dai_common_trigger(), already processing atomic trigger");
		ret = -EAGAIN;
	} else {
		/* Count down the number of remaining DAIs required
		 * to receive the trigger command before atomic trigger
		 * takes place
		 */
		group->trigger_counter--;
		comp_dbg(dev, "dai_common_trigger(), trigger counter %d, group %d",
			 group->trigger_counter, group->group_id);

		if (!group->trigger_counter) {
			/* The counter has reached 0, which means
			 * all DAIs have received the same trigger command
			 * and we may begin the actual trigger process
			 * synchronously.
			 */

			irq_local_disable(irq_flags);
			notifier_event(group, NOTIFIER_ID_DAI_TRIGGER,
				       BIT(cpu_get_id()), NULL, 0);
			irq_local_enable(irq_flags);

			/* return error of last trigger */
			ret = group->trigger_ret;
		}
	}

	return ret;
}

static int dai_comp_trigger(struct comp_dev *dev, int cmd)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	return dai_common_trigger(dd, dev, cmd);
}

/* process and copy stream data from multiple DMA source buffers to sink buffer */
int dai_zephyr_multi_endpoint_copy(struct dai_data **dd, struct comp_dev *dev,
				   struct comp_buffer *multi_endpoint_buffer,
				   int num_endpoints)
{
	uint32_t avail_bytes = UINT32_MAX;
	uint32_t free_bytes = UINT32_MAX;
	uint32_t frames;
	uint32_t src_frames, sink_frames;
	uint32_t frame_bytes;
	int ret, i;
	int direction;

	if (!num_endpoints || !dd || !multi_endpoint_buffer)
		return 0;

	frame_bytes = audio_stream_frame_bytes(&dd[0]->dma_buffer->stream);

	direction = dev->direction;

	/* Consume or produce everything free or available */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		free_bytes = audio_stream_get_free_bytes(&multi_endpoint_buffer->stream);
		avail_bytes = FAKE_DAI_MAX_BYTES_FREE_AVAIL;
	} else {
		free_bytes = FAKE_DAI_MAX_BYTES_FREE_AVAIL;
		avail_bytes = audio_stream_get_free_bytes(&multi_endpoint_buffer->stream);
	}

	/* calculate minimum size to copy */
	if (direction == SOF_IPC_STREAM_PLAYBACK) {
		src_frames = audio_stream_get_avail_frames(&multi_endpoint_buffer->stream);
		sink_frames = free_bytes / frame_bytes;
	} else {
		src_frames = avail_bytes / frame_bytes;
		sink_frames = audio_stream_get_free_frames(&multi_endpoint_buffer->stream);
	}

	frames = MIN(src_frames, sink_frames);

	/* limit bytes per copy to one period for the whole pipeline in order to avoid high load
	 * spike if FAST_MODE is enabled, then one period limitation is omitted. All dd's have the
	 * same period_bytes, so use the period_bytes from dd[0]
	 */
	if (!(dd[0]->ipc_config.feature_mask & BIT(IPC4_COPIER_FAST_MODE)))
		frames = MIN(frames, dd[0]->period_bytes / frame_bytes);
	comp_dbg(dev, "dai_zephyr_multi_endpoint_copy(), dir: %d copy frames= 0x%x",
		 dev->direction, frames);

	if (direction == SOF_IPC_STREAM_PLAYBACK) {
		frame_bytes = audio_stream_frame_bytes(&multi_endpoint_buffer->stream);
		buffer_stream_invalidate(multi_endpoint_buffer, frames * frame_bytes);
	}

	for (i = 0; i < num_endpoints; i++) {
		enum dma_cb_status status;
		uint32_t copy_bytes;

		/* trigger optional DAI_TRIGGER_COPY which prepares dai to copy */
		ret = fake_dai_trigger(dd[i]->dai->dev, direction, DAI_TRIGGER_COPY);
		if (ret < 0)
			comp_warn(dev, "dai_zephyr_multi_endpoint_copy(): dai trigger copy failed");

		status = dai_dma_multi_endpoint_cb(dd[i], dev, frames, multi_endpoint_buffer);
		if (status == DMA_CB_STATUS_END)
			dma_stop(dd[i]->chan->dma->z_dev, dd[i]->chan->index);

		copy_bytes = frames * audio_stream_frame_bytes(&dd[i]->dma_buffer->stream);

		fake_dai_dma_position_update(dd[i], dev);
	}

	frame_bytes = audio_stream_frame_bytes(&multi_endpoint_buffer->stream);
	if (direction == SOF_IPC_STREAM_PLAYBACK) {
		comp_update_buffer_consume(multi_endpoint_buffer, frames * frame_bytes);
	} else {
		buffer_stream_writeback(multi_endpoint_buffer, frames * frame_bytes);
		comp_update_buffer_produce(multi_endpoint_buffer, frames * frame_bytes);
	}

	return 0;
}

static void set_new_local_buffer(struct dai_data *dd, struct comp_dev *dev)
{
	uint32_t dma_fmt = audio_stream_get_frm_fmt(&dd->dma_buffer->stream);
	uint32_t local_fmt;

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		dd->local_buffer = list_first_item(&dev->bsource_list,
						   struct comp_buffer,
						   sink_list);
	else
		dd->local_buffer = list_first_item(&dev->bsink_list,
						   struct comp_buffer,
						   source_list);

	local_fmt = audio_stream_get_frm_fmt(&dd->local_buffer->stream);

	dd->process = pcm_get_conversion_function(local_fmt, dma_fmt);

	if (!dd->process) {
		comp_err(dev, "converter function NULL: local fmt %d dma fmt %d\n",
			 local_fmt, dma_fmt);
		dd->local_buffer = NULL;
	}
}

/* copy and process stream data from source to sink buffers */
int dai_common_copy(struct dai_data *dd, struct comp_dev *dev, pcm_converter_func *converter)
{
	uint32_t sampling = dd->sampling;
	uint32_t avail_bytes;
	uint32_t free_bytes;
	uint32_t copy_bytes;
	uint32_t src_samples;
	uint32_t sink_samples;
	uint32_t samples = UINT32_MAX;
	int ret;

	/* Consume or produce everything free or available */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		free_bytes = audio_stream_get_avail_bytes(&dd->local_buffer->stream);
		avail_bytes = FAKE_DAI_MAX_BYTES_FREE_AVAIL;
	} else {
		free_bytes = FAKE_DAI_MAX_BYTES_FREE_AVAIL;
		avail_bytes = audio_stream_get_free_bytes(&dd->local_buffer->stream);
	}

	/* handle module runtime unbind */
	if (!dd->local_buffer) {
		set_new_local_buffer(dd, dev);

		if (!dd->local_buffer) {
			comp_warn(dev, "dai_zephyr_copy(): local buffer unbound, cannot copy");
			return 0;
		}
	}

	/* calculate minimum size to copy */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		src_samples = audio_stream_get_avail_samples(&dd->local_buffer->stream);
		sink_samples = free_bytes / sampling;
		samples = MIN(src_samples, sink_samples);
	} else {
		struct list_item *sink_list;

		src_samples = avail_bytes / sampling;

		/*
		 * there's only one sink buffer in the case of endpoint DAI devices created by
		 * a DAI copier and it is chosen as the dd->local buffer
		 */
		if (!converter) {
			sink_samples = audio_stream_get_free_samples(&dd->local_buffer->stream);
			samples = MIN(samples, sink_samples);
		} else {
			/*
			 * In the case of capture DAI's with multiple sink buffers, compute the
			 * minimum number of samples based on the DMA avail_bytes and the free
			 * samples in all active sink buffers.
			 */
			list_for_item(sink_list, &dev->bsink_list) {
				struct comp_dev *sink_dev;
				struct comp_buffer *sink;

				sink = container_of(sink_list, struct comp_buffer, source_list);
				sink_dev = sink->sink;

				if (sink_dev && sink_dev->state == COMP_STATE_ACTIVE) {
					sink_samples =
						audio_stream_get_free_samples(&sink->stream);
					samples = MIN(samples, sink_samples);
				}
			}
		}

		samples = MIN(samples, src_samples);
	}

	/* limit bytes per copy to one period for the whole pipeline
	 * in order to avoid high load spike
	 * if FAST_MODE is enabled, then one period limitation is omitted
	 */
	if (!dd->fast_mode)
		samples = MIN(samples, dd->period_bytes / sampling);

	copy_bytes = samples * sampling;

	comp_dbg(dev, "dai_common_copy(), dir: %d copy_bytes= 0x%x",
		 dev->direction, copy_bytes);

#if CONFIG_DAI_VERBOSE_GLITCH_WARNINGS
	/* Check possibility of glitch occurrence */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK &&
	    copy_bytes + avail_bytes < dd->period_bytes)
		comp_warn(dev, "dai_common_copy(): Copy_bytes %d + avail bytes %d < period bytes %d, possible glitch",
			  copy_bytes, avail_bytes, dd->period_bytes);
	else if (dev->direction == SOF_IPC_STREAM_CAPTURE &&
		 copy_bytes + free_bytes < dd->period_bytes)
		comp_warn(dev, "dai_common_copy(): Copy_bytes %d + free bytes %d < period bytes %d, possible glitch",
			  copy_bytes, free_bytes, dd->period_bytes);
#endif

	/* return if nothing to copy */
	if (!copy_bytes) {
#if CONFIG_DAI_VERBOSE_GLITCH_WARNINGS
		comp_warn(dev, "dai_zephyr_copy(): nothing to copy");
#endif
		return 0;
	}

	/* trigger optional DAI_TRIGGER_COPY which prepares dai to copy */
	ret = fake_dai_trigger(dd->dai->dev, dev->direction, DAI_TRIGGER_COPY);
	if (ret < 0)
		comp_warn(dev, "dai_common_copy(): dai trigger copy failed");

	if (dai_dma_cb(dd, dev, copy_bytes, converter) == DMA_CB_STATUS_END)
		dma_stop(dd->chan->dma->z_dev, dd->chan->index);

	fake_dai_dma_position_update(dd, dev);

	return ret;
}

static int dai_copy(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/*
	 * DAI devices will only ever have 1 sink, so no need to pass an array of PCM converter
	 * functions. The default one to use is set in dd->process.
	 */
	return dai_common_copy(dd, dev, NULL);
}

/**
 * \brief Get DAI parameters and configure timestamping
 * \param[in, out] dev DAI device.
 * \return Error code.
 *
 * This function retrieves various DAI parameters such as type, direction, index, and DMA
 * controller information those are needed when configuring HW timestamping. Note that
 * DAI must be prepared before this function is used (for DMA information). If not, an error
 * is returned.
 */
int dai_common_ts_config_op(struct dai_data *dd, struct comp_dev *dev)
{
	struct ipc_config_dai *dai = &dd->ipc_config;
	struct dai_ts_cfg *cfg = &dd->ts_config;

	comp_dbg(dev, "dai_ts_config()");
	if (!dd->chan) {
		comp_err(dev, "dai_ts_config(), No DMA channel information");
		return -EINVAL;
	}

	switch (dai->type) {
	case SOF_DAI_INTEL_SSP:
		cfg->type = DAI_INTEL_SSP;
		break;
	case SOF_DAI_INTEL_ALH:
		cfg->type = DAI_INTEL_ALH;
		break;
	case SOF_DAI_INTEL_DMIC:
		cfg->type = DAI_INTEL_DMIC;
		break;
	default:
		comp_err(dev, "dai_ts_config(), not supported dai type");
		return -EINVAL;
	}

	cfg->direction = dai->direction;
	cfg->index = dd->dai->index;
	cfg->dma_id = dd->dma->plat_data.id;
	cfg->dma_chan_index = dd->chan->index;
	cfg->dma_chan_count = dd->dma->plat_data.channels;

	return dai_ts_config(dd->dai->dev, cfg);
}

static int dai_ts_config_op(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	return dai_common_ts_config_op(dd, dev);
}

int dai_common_ts_start(struct dai_data *dd, struct comp_dev *dev)
{
	return dai_ts_start(dd->dai->dev, (struct dai_ts_cfg *)&dd->ts_config);
}

static int dai_ts_start_op(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	comp_dbg(dev, "dai_ts_start()");
	return dai_common_ts_start(dd, dev);
}

int dai_common_ts_get(struct dai_data *dd, struct comp_dev *dev, struct dai_ts_data *tsd)
{
	struct dai_ts_cfg *cfg = (struct dai_ts_cfg *)&dd->ts_config;

	return dai_ts_get(dd->dai->dev, cfg, tsd);
}

static int dai_ts_get_op(struct comp_dev *dev, struct dai_ts_data *tsd)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	comp_dbg(dev, "dai_ts_get()");

	return dai_common_ts_get(dd, dev, tsd);
}

int dai_common_ts_stop(struct dai_data *dd, struct comp_dev *dev)
{
	return dai_ts_stop(dd->dai->dev, (struct dai_ts_cfg *)&dd->ts_config);
}

static int dai_ts_stop_op(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	comp_dbg(dev, "dai_ts_stop()");

	return dai_common_ts_stop(dd, dev);
}

uint32_t dai_get_init_delay_ms(struct dai *dai)
{
	return 0;
}

static uint64_t dai_get_processed_data(struct comp_dev *dev, uint32_t stream_no, bool input)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	uint64_t ret = 0;
	bool source = dev->direction == SOF_IPC_STREAM_CAPTURE;

	/* Return value only if direction and stream number match.
	 * The dai supports only one stream.
	 */
	if (stream_no == 0 && source == input)
		ret = dd->total_data_processed;

	return ret;
}

#ifdef CONFIG_IPC_MAJOR_4
int dai_zephyr_unbind(struct dai_data *dd, struct comp_dev *dev, void *data)
{
	struct ipc4_module_bind_unbind *bu;
	int buf_id;

	bu = (struct ipc4_module_bind_unbind *)data;
	buf_id = IPC4_COMP_ID(bu->extension.r.src_queue, bu->extension.r.dst_queue);

	if (dd && dd->local_buffer) {
		if (buf_get_id(dd->local_buffer) == buf_id) {
			comp_dbg(dev, "dai_zephyr_unbind: local_buffer %x unbound", buf_id);
			dd->local_buffer = NULL;
		}
	}

	return 0;
}
#endif /* CONFIG_IPC_MAJOR_4 */

static const struct comp_driver comp_dai = {
	.type	= SOF_COMP_DAI,
	.uid	= SOF_RT_UUID(dai_comp_uuid),
	.tctx	= &dai_comp_tr,
	.ops	= {
		.create				= dai_new,
		.free				= dai_free,
		.params				= dai_params,
		.dai_get_hw_params		= dai_comp_get_hw_params,
		.trigger			= dai_comp_trigger,
		.copy				= dai_copy,
		.prepare			= dai_prepare,
		.reset				= dai_reset,
		.position			= dai_position,
		.dai_config			= dai_config,
		.dai_ts_config			= dai_ts_config_op,
		.dai_ts_start			= dai_ts_start_op,
		.dai_ts_stop			= dai_ts_stop_op,
		.dai_ts_get			= dai_ts_get_op,
		.get_total_data_processed	= dai_get_processed_data,
},
};

static SHARED_DATA struct comp_driver_info comp_dai_info = {
	.drv = &comp_dai,
};

UT_STATIC void sys_comp_dai_init(void)
{
	comp_register(platform_shared_get(&comp_dai_info, sizeof(comp_dai_info)));
}

DECLARE_MODULE(sys_comp_dai_init);
SOF_MODULE_INIT(dai, sys_comp_dai_init);

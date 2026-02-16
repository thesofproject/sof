// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h> /* for SHARED_DATA */
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/trig.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/tone.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "tone.h"

SOF_DEFINE_REG_UUID(tone);
LOG_MODULE_DECLARE(tone, CONFIG_SOF_LOG_LEVEL);

static const struct comp_driver comp_tone;

static struct comp_dev *tone_new(const struct comp_driver *drv,
				 const struct comp_ipc_config *config,
				 const void *spec)
{
	struct comp_dev *dev;
	const struct ipc_config_tone *ipc_tone = spec;
	struct comp_data *cd;
	int i;

	comp_cl_info(&comp_tone, "tone_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd) {
		comp_free_device(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	cd->tone_func = tone_s32_default;

	cd->rate = ipc_tone->sample_rate;

	/* Reset tone generator and set channels volumes to default */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		tonegen_reset(&cd->sg[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void tone_free(struct comp_dev *dev)
{
	struct tone_data *td = comp_get_drvdata(dev);

	comp_info(dev, "entry");

	rfree(td);
	comp_free_device(dev);
}

/* set component audio stream parameters */
static int tone_params(struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb, *sinkb;

	sourceb = comp_dev_get_first_data_producer(dev);
	sinkb = comp_dev_get_first_data_consumer(dev);
	if (!sourceb || !sinkb) {
		comp_err(dev, "no source or sink buffer");
		return -ENOTCONN;
	}

	comp_info(dev, "config->frame_fmt = %u",
		  dev->ipc_config.frame_fmt);

	/* Tone supports only S32_LE PCM format atm */
	if (dev->ipc_config.frame_fmt != SOF_IPC_FRAME_S32_LE)
		return -EINVAL;

	audio_stream_set_frm_fmt(&sourceb->stream, dev->ipc_config.frame_fmt);
	audio_stream_set_frm_fmt(&sinkb->stream, dev->ipc_config.frame_fmt);

	/* calculate period size based on config */
	cd->period_bytes = dev->frames *
			   audio_stream_frame_bytes(&sourceb->stream);

	return 0;
}

#if CONFIG_IPC_MAJOR_3
static int tone_cmd_get_value(struct comp_dev *dev,
			      struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;

	comp_info(dev, "entry");

	if (cdata->type != SOF_CTRL_TYPE_VALUE_CHAN_GET) {
		comp_err(dev, "wrong cdata->type: %u",
			 cdata->type);
		return -EINVAL;
	}

	if (cdata->cmd == SOF_CTRL_CMD_SWITCH) {
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = !cd->sg[j].mute;
			comp_info(dev, "j = %u, cd->sg[j].mute = %u",
				  j, cd->sg[j].mute);
		}
	}
	return 0;
}

static int tone_cmd_set_value(struct comp_dev *dev,
			      struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;
	uint32_t ch;
	bool val;

	if (cdata->type != SOF_CTRL_TYPE_VALUE_CHAN_SET) {
		comp_err(dev, "wrong cdata->type: %u",
			 cdata->type);
		return -EINVAL;
	}

	if (cdata->cmd == SOF_CTRL_CMD_SWITCH) {
		comp_info(dev, "SOF_CTRL_CMD_SWITCH");
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			comp_info(dev, "SOF_CTRL_CMD_SWITCH, ch = %u, val = %u",
				  ch, val);
			if (ch >= PLATFORM_MAX_CHANNELS) {
				comp_err(dev, "ch >= PLATFORM_MAX_CHANNELS");
				return -EINVAL;
			}

			if (val)
				tonegen_unmute(&cd->sg[ch]);
			else
				tonegen_mute(&cd->sg[ch]);
		}
	} else {
		comp_err(dev, "invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

static int tone_cmd_set_data(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_ctrl_value_comp *compv;
	int i;
	uint32_t ch;
	uint32_t val;

	comp_info(dev, "entry");

	if (cdata->type != SOF_CTRL_TYPE_VALUE_COMP_SET) {
		comp_err(dev, "wrong cdata->type: %u",
			 cdata->type);
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_info(dev, "SOF_CTRL_CMD_ENUM, cdata->index = %u",
			  cdata->index);
		compv = (struct sof_ipc_ctrl_value_comp *)ASSUME_ALIGNED(&cdata->data->data, 4);

		for (i = 0; i < (int)cdata->num_elems; i++) {
			ch = compv[i].index;
			val = compv[i].svalue;
			comp_info(dev, "SOF_CTRL_CMD_ENUM, ch = %u, val = %u",
				  ch, val);
			switch (cdata->index) {
			case SOF_TONE_IDX_FREQUENCY:
				comp_info(dev, "SOF_TONE_IDX_FREQUENCY");
				tonegen_update_f(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_AMPLITUDE:
				comp_info(dev, "SOF_TONE_IDX_AMPLITUDE");
				tonegen_set_a(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_FREQ_MULT:
				comp_info(dev, "SOF_TONE_IDX_FREQ_MULT");
				tonegen_set_freq_mult(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_AMPL_MULT:
				comp_info(dev, "SOF_TONE_IDX_AMPL_MULT");
				tonegen_set_ampl_mult(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_LENGTH:
				comp_info(dev, "SOF_TONE_IDX_LENGTH");
				tonegen_set_length(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_PERIOD:
				comp_info(dev, "SOF_TONE_IDX_PERIOD");
				tonegen_set_period(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_REPEATS:
				comp_info(dev, "SOF_TONE_IDX_REPEATS");
				tonegen_set_repeats(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_LIN_RAMP_STEP:
				comp_info(dev, "SOF_TONE_IDX_LIN_RAMP_STEP");
				tonegen_set_linramp(&cd->sg[ch], val);
				break;
			default:
				comp_err(dev, "invalid cdata->index");
				return -EINVAL;
			}
		}
		break;
	default:
		comp_err(dev, "invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int tone_cmd(struct comp_dev *dev, int cmd, void *data,
		    int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "entry");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = tone_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_SET_VALUE:
		ret = tone_cmd_set_value(dev, cdata);
		break;
	case COMP_CMD_GET_VALUE:
		ret = tone_cmd_get_value(dev, cdata, max_data_size);
		break;
	}

	return ret;
}
#endif

static int tone_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "entry");

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int tone_copy(struct comp_dev *dev)
{
	struct comp_buffer *sink;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t free;
	int ret = 0;

	comp_dbg(dev, "entry");

	/* tone component sink buffer */
	sink = comp_dev_get_first_data_consumer(dev);
	free = audio_stream_get_free_bytes(&sink->stream);

	/* Test that sink has enough free frames. Then run once to maintain
	 * low latency and steady load for tones.
	 */
	if (free >= cd->period_bytes) {
		/* create tone */
		cd->tone_func(dev, &sink->stream, dev->frames);
		buffer_stream_writeback(sink, cd->period_bytes);

		/* calc new free and available */
		comp_update_buffer_produce(sink, cd->period_bytes);

		ret = dev->frames;
	}

	return ret;
}

static int tone_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	int32_t f;
	int32_t a;
	int ret;
	int i;

	comp_info(dev, "entry");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	sourceb = comp_dev_get_first_data_producer(dev);
	if (!sourceb) {
		comp_err(dev, "no source buffer");
		return -ENOTCONN;
	}

	cd->channels = audio_stream_get_channels(&sourceb->stream);
	comp_info(dev, "cd->channels = %u, cd->rate = %u",
		  cd->channels, cd->rate);

	for (i = 0; i < cd->channels; i++) {
		f = tonegen_get_f(&cd->sg[i]);
		a = tonegen_get_a(&cd->sg[i]);
		if (tonegen_init(&cd->sg[i], cd->rate, f, a) < 0) {
			comp_set_state(dev, COMP_TRIGGER_RESET);
			return -EINVAL;
		}
	}

	return 0;
}

static int tone_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i;

	comp_info(dev, "entry");

	/* Initialize with the defaults */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		tonegen_reset(&cd->sg[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

DECLARE_TR_CTX(tone_tr, SOF_UUID(tone_uuid), LOG_LEVEL_INFO);

static const struct comp_driver comp_tone = {
	.type = SOF_COMP_TONE,
	.uid = SOF_RT_UUID(tone_uuid),
	.tctx = &tone_tr,
	.ops = {
		.create = tone_new,
		.free = tone_free,
		.params = tone_params,
#if CONFIG_IPC_MAJOR_3
		.cmd = tone_cmd,
#endif
		.trigger = tone_trigger,
		.copy = tone_copy,
		.prepare = tone_prepare,
		.reset = tone_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_tone_info = {
	.drv = &comp_tone,
};

UT_STATIC void sys_comp_tone_init(void)
{
	comp_register(platform_shared_get(&comp_tone_info,
					  sizeof(comp_tone_info)));
}

DECLARE_MODULE(sys_comp_tone_init);
SOF_MODULE_INIT(tone, sys_comp_tone_init);

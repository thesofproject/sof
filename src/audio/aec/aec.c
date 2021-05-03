// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/aec/aec.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define AEC_BLK_LENGTH 4
#define AEC_MAX_SOURCES 2 /* Microphone and reference streams */

static const struct comp_driver comp_aec;

/* UUID 2ca424c0-7e1c-4b0a-afca-43de944705d3 */
DECLARE_SOF_RT_UUID("aec", aec_uuid, 0x2ca424c0, 0x7e1c, 0x4b0a, 0xaf, 0xca,
		    0x43, 0xde, 0x94, 0x47, 0x05, 0xd3);

DECLARE_TR_CTX(aec_tr, SOF_UUID(aec_uuid), LOG_LEVEL_INFO);

/* Generic processing */

#if CONFIG_FORMAT_S16LE

static void aec_s16_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	int64_t mix;
	int32_t ref;
	int16_t out;
	int16_t *x;
	int16_t *y;
	int16_t *r16;
	int32_t *r32;
	int in_idx;
	int ref_idx;
	int ref_nch;
	int i;
	int j;
	int nch = sources[0]->channels;
	int out_idx = 0;

	if (cd->ref_active)
		ref_nch = sources[1]->channels;

	/* Input from selected input and reference channels, same output to every channel */
	in_idx = cd->source_channel;
	ref_idx = cd->reference_channel;
	for (i = 0; i < frames; i++) {
		x = audio_stream_read_frag_s16(sources[0], in_idx);
		mix = *x << 16;
		in_idx += nch;
		if (cd->ref_active) {
			if (cd->ref_32bits) {
				r32 = audio_stream_read_frag_s32(sources[1], ref_idx);
				ref = *r32;
			} else {
				r16 = audio_stream_read_frag_s16(sources[1], ref_idx);
				ref = *r16;
			}
			ref_idx += ref_nch;
			mix = mix - (ref << cd->ref_shift);
		}

		out = sat_int16(mix >> 16);
		for (j = 0; j < nch; j++) {
			y = audio_stream_write_frag_s16(sink, out_idx);
			*y = out;
			out_idx++;
		}
	}
}

#endif

#if CONFIG_FORMAT_S24LE

static void aec_s24_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	int64_t mix;
	int32_t out;
	int32_t ref;
	int32_t *x;
	int32_t *y;
	int32_t *r32;
	int16_t *r16;
	int in_idx;
	int ref_idx;
	int ref_nch;
	int i;
	int j;
	int nch = sources[0]->channels;
	int out_idx = 0;

	if (cd->ref_active)
		ref_nch = sources[1]->channels;

	/* Input from selected input and reference channels, same output to every channel */
	in_idx = cd->source_channel;
	ref_idx = cd->reference_channel;
	for (i = 0; i < frames; i++) {
		x = audio_stream_read_frag_s32(sources[0], in_idx);
		mix = *x << 8;
		in_idx += nch;
		if (cd->ref_active) {
			if (cd->ref_32bits) {
				r32 = audio_stream_read_frag_s32(sources[1], ref_idx);
				ref = *r32;
			} else {
				r16 = audio_stream_read_frag_s16(sources[1], ref_idx);
				ref = *r16;
			}

			ref_idx += ref_nch;
			mix = mix - (ref << cd->ref_shift);
		}

		out = sat_int24(mix >> 8);
		for (j = 0; j < nch; j++) {
			y = audio_stream_write_frag_s32(sink, out_idx);
			*y = out;
			out_idx++;
		}
	}
}

#endif

#if CONFIG_FORMAT_S32LE

static void aec_s32_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	int64_t mix;
	int32_t out;
	int32_t ref;
	int32_t *x;
	int32_t *y;
	int32_t *r32;
	int16_t *r16;
	int in_idx;
	int ref_idx;
	int ref_nch;
	int i;
	int j;
	int nch = sources[0]->channels;
	int out_idx = 0;

	if (cd->ref_active)
		ref_nch = sources[1]->channels;

	/* Input from selected input and reference channels, same output to every channel */
	in_idx = cd->source_channel;
	ref_idx = cd->reference_channel;
	for (i = 0; i < frames; i++) {
		x = audio_stream_read_frag_s32(sources[0], in_idx);
		mix = *x;
		in_idx += nch;
		if (cd->ref_active) {
			if (cd->ref_32bits) {
				r32 = audio_stream_read_frag_s32(sources[1], ref_idx);
				ref = *r32;
			} else {
				r16 = audio_stream_read_frag_s16(sources[1], ref_idx);
				ref = *r16;
			}

			ref_idx += ref_nch;
			mix = mix - (ref << cd->ref_shift);
		}

		out = sat_int32(mix);
		for (j = 0; j < nch; j++) {
			y = audio_stream_write_frag_s32(sink, out_idx);
			*y = out;
			out_idx++;
		}
	}
}

#endif

/* Processing functions table */
const struct aec_func_map aec_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, aec_s16_default },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, aec_s24_default },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, aec_s32_default },
#endif
};

const size_t aec_fncount = ARRAY_SIZE(aec_fnmap);

/*
 * End of AEC setup code. Next the standard component methods.
 */

static struct comp_dev *aec_new(const struct comp_driver *drv, struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_process *aec;
	struct sof_ipc_comp_process *ipc_aec = (struct sof_ipc_comp_process *)comp;
	int ret;

	comp_cl_info(&comp_aec, "aec_new()");

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	aec = COMP_GET_IPC(dev, sof_ipc_comp_process);
	ret = memcpy_s(aec, sizeof(*aec), ipc_aec, sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_aec, "aec_new(): comp_data_blob_handler_new() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	/* Allocate and make a copy of the coefficients blob and reset IIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	ret = comp_init_data_blob(cd->model_handler, ipc_aec->size, ipc_aec->data);
	if (ret < 0) {
		comp_cl_err(&comp_aec, "aec_new(): comp_init_data_blob() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	/* Component defaults */
	cd->source_channel = 0;
	cd->reference_channel = 0;
	cd->count = 0;

	/* Done. */
	dev->state = COMP_STATE_READY;
	return dev;
}

static void aec_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "aec_free()");

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int aec_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int ret;

	comp_info(dev, "aec_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "aec_params() error: comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

static int aec_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int max_size)
{
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "aec_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		break;
	default:
		comp_err(dev, "aec_cmd_set_data() error: invalid command %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int aec_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "aec_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		break;
	default:
		comp_err(dev, "aec_cmd_set_data() error: invalid command %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int aec_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	comp_info(dev, "aec_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = aec_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = aec_cmd_get_data(dev, cdata, max_data_size);
		break;
	}

	return ret;
}

static int aec_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "aec_trigger()");

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int aec_copy(struct comp_dev *dev)
{
	struct list_item *blist;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	int frames;
	int num_blocks;
	int ref_bytes;
	int sink_bytes;
	int source_bytes;
	const struct audio_stream *sources_stream[AEC_MAX_SOURCES] = { NULL };
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sources[AEC_MAX_SOURCES] = { NULL };
	uint32_t flags = 0;
	int num_sources = 0;

	comp_dbg(dev, "aec_copy()");

	/* align source streams with their respective configurations */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		buffer_lock(source, &flags);
		if (source->source->state == dev->state) {
			if (num_sources < AEC_MAX_SOURCES) {
				sources[num_sources] = source;
				sources_stream[num_sources] = &source->stream;
				num_sources++;
			} else {
				buffer_unlock(source, flags);
			}
		} else {
			buffer_unlock(source, flags);
		}
	}

	/* check if there are any sources active */
	if (num_sources == 0 || num_sources > 2)
		return -EINVAL;

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	buffer_lock(sink, &flags);
	frames = audio_stream_avail_frames(sources_stream[0], &sink->stream);
	buffer_unlock(sources[0], flags);
	if (num_sources == 2) {
		frames = MIN(frames, audio_stream_avail_frames(sources_stream[1], &sink->stream));
		buffer_unlock(sources[1], flags);
		cd->ref_format = sources_stream[1]->frame_fmt;
		cd->ref_active = true;

		/* Needed reference shift for 32 bit filter calculate */
		switch (cd->ref_format) {
		case SOF_IPC_FRAME_S32_LE:
			cd->ref_shift = 0;
			cd->ref_32bits = true;
			break;
		case SOF_IPC_FRAME_S24_4LE:
			cd->ref_shift = 8;
			cd->ref_32bits = true;
			break;
		case SOF_IPC_FRAME_S16_LE:
			cd->ref_shift = 16;
			cd->ref_32bits = false;
			break;
		default:
			comp_err(dev, "aec_copy(): Invalid reference format %d.",
				 cd->ref_format);
			return -EINVAL;
		}

	} else {
		cd->ref_active = false;
	}

	/* Process integer multiple of AEC internal block length */
	num_blocks = frames / AEC_BLK_LENGTH;
	frames = num_blocks * AEC_BLK_LENGTH;

	if (cd->count) {
		cd->count--;
	} else {
		comp_info(dev, "aec_copy(): num_sources = %d, frames = %d, ref_shift = %d",
			  num_sources, frames, cd->ref_shift);
		cd->count = 999;
	}

	if (frames) {
		/* Run processing function */
		cd->aec_func(dev, sources_stream, &sink->stream, frames);

		/* Track consume and produce */
		source_bytes = frames * audio_stream_frame_bytes(sources_stream[0]);
		sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);
		comp_update_buffer_consume(sources[0], source_bytes);
		comp_update_buffer_produce(sink, sink_bytes);
		if (cd->ref_active) {
			ref_bytes = frames * audio_stream_frame_bytes(sources_stream[1]);
			comp_update_buffer_consume(sources[1], ref_bytes);
		}
	}

	return 0;
}

static int aec_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;
	int ret;

	comp_info(dev, "aec_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Get sink data format */
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	cd->sink_format = sinkb->stream.frame_fmt;

	/* Check source and sink PCM format and get copy function */
	comp_info(dev, "aec_prepare(), sink_format=%d", cd->sink_format);
	cd->aec_func = aec_find_func(cd->sink_format);
	if (!cd->aec_func) {
		comp_err(dev, "aec_prepare(): No suitable processing function found.");
		goto err;
	}

	/* Initialize AEC */
	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int aec_reset(struct comp_dev *dev)
{
	comp_info(dev, "aec_reset()");
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_aec = {
	.uid = SOF_RT_UUID(aec_uuid),
	.tctx = &aec_tr,
	.ops = {
		.create = aec_new,
		.free = aec_free,
		.params = aec_params,
		.cmd = aec_cmd,
		.trigger = aec_trigger,
		.copy = aec_copy,
		.prepare = aec_prepare,
		.reset = aec_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_aec_info = {
	.drv = &comp_aec,
};

UT_STATIC void sys_comp_aec_init(void)
{
	comp_register(platform_shared_get(&comp_aec_info, sizeof(comp_aec_info)));
}

DECLARE_MODULE(sys_comp_aec_init);

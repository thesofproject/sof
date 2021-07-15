// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Realtek Semiconductor Corp. All rights reserved.
//
// Author: Ming Jen Tai <mingjen_tai@realtek.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/rtnr/rtnr.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
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

#include <sof/audio/rtnr/rtklib/include/RTK_MA_API.h>


#define SAMPLING_RATE		16000
#define PROCESS_RATE		16000
#define PRESET_SIZE			1040
#define MicNum				2
#define SpkNum				2

#if SAMPLING_RATE == 48000
#define BLOCKLENGTH	768
#elif SAMPLING_RATE == 16000
#define BLOCKLENGTH	256
#elif SAMPLING_RATE == 8000
#define BLOCKLENGTH	128
#endif // SAMPLING_RATE


#define RTNR_BLK_LENGTH 4
#define RTNR_MAX_SOURCES 1 /* Microphone stream */

/*	MIC input is at index 0 */
#define RTNR_MIC_SOURCE_INDEX (0)

static const struct comp_driver comp_rtnr;

/** \brief RTNR processing functions map item. */
struct rtnr_func_map {
	enum sof_ipc_frame fmt; /**< source frame format */
	rtnr_func func; /**< processing function */
};

#if CONFIG_FORMAT_S16LE
static void rtnr_s16_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames);
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void rtnr_s24_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames);
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void rtnr_s32_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames);
#endif /* CONFIG_FORMAT_S32LE */

/* Processing functions table */
const struct rtnr_func_map rtnr_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, rtnr_s16_default },
#endif // CONFIG_FORMAT_S16LE
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, rtnr_s24_default },
#endif // CONFIG_FORMAT_S24LE
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, rtnr_s32_default },
#endif// CONFIG_FORMAT_S32LE
};

const size_t rtnr_fncount = ARRAY_SIZE(rtnr_fnmap);

/* UUID 5c7ca334-e15d-11eb-ba80-0242ac130004 */
DECLARE_SOF_RT_UUID("rtnr", rtnr_uuid, 0x5c7ca334, 0xe15d, 0x11eb, 0xba, 0x80,
		    0x02, 0x42, 0xac, 0x13, 0x00, 0x04);

DECLARE_TR_CTX(rtnr_tr, SOF_UUID(rtnr_uuid), LOG_LEVEL_INFO);

/* Generic processing */


void myPrintf(int a, int b, int c, int d, int e)
{
	if (a == 0xa)
		comp_cl_info(&comp_rtnr, "myPrintf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
			b, c, d, e);
	else if	(a == 0xb)
		comp_cl_info(&comp_rtnr, "myPrintf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
			b, c, d, e);
	else if	(a == 0xc)
		comp_cl_warn(&comp_rtnr, "myPrintf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
			b, c, d, e);
	else if	(a == 0xd)
		comp_cl_dbg(&comp_rtnr, "myPrintf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
			b, c, d, e);
	else if	(a == 0xe)
		comp_cl_err(&comp_rtnr, "myPrintf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
			b, c, d, e);
}

void *rtk_rballoc(unsigned int flags, unsigned int caps, unsigned int bytes)
{
	return rballoc(flags, caps, bytes);
}

void rtk_rfree(void *ptr)
{
	rfree(ptr);
}

#if CONFIG_FORMAT_S16LE

static void rtnr_s16_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	RTKMA_API_S16_Default((void *)&cd->rtk_agl, sources, sink, frames,
	0, 0, 0,
	0, 0);
}

#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE

static void rtnr_s24_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	RTKMA_API_S24_Default((void *)&cd->rtk_agl, sources, sink, frames,
	0, 0, 0,
	0, 0);
}

#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE

static void rtnr_s32_default(struct comp_dev *dev, const struct audio_stream **sources,
			    struct audio_stream *sink, int frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	RTKMA_API_S32_Default((void *)&cd->rtk_agl, sources, sink, frames,
	0, 0, 0,
	0, 0);
}

#endif /* CONFIG_FORMAT_S32LE */


/**
 * \brief Retrieves an RTNR processing function matching
 *	  the source buffer's frame format.
 * \param fmt the frames' format of the source and sink buffers
 */
static rtnr_func rtnr_find_func(enum sof_ipc_frame fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < rtnr_fncount; i++) {
		if (fmt == rtnr_fnmap[i].fmt)
			return rtnr_fnmap[i].func;
	}

	return NULL;
}

/*
 * End of RTNR setup code. Next the standard component methods.
 */

static struct comp_dev *rtnr_new(const struct comp_driver *drv,
		struct comp_ipc_config *config,
		void *spec)
{
	struct ipc_config_process *ipc_rtnr = spec;
	struct comp_dev *dev;
	struct comp_data *cd;
	size_t bs = ipc_rtnr->size;
	int ret;

	comp_cl_info(&comp_rtnr, "rtnr_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto err;

	comp_set_drvdata(dev, cd);

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_rtnr, "rtnr_new(): comp_data_blob_handler_new() failed.");
		goto cd_fail;
	}

	/* Get configuration data */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_rtnr->data);
	if (ret < 0) {
		comp_cl_err(&comp_rtnr, "rtnr_new(): comp_init_data_blob() failed.");
		goto cd_fail;
	}

	/* Component defaults */
	cd->source_channel = 0;

	RTKMA_API_Context_Create((void *)&cd->rtk_agl);

	/* Done. */
	dev->state = COMP_STATE_READY;
	return dev;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);

err:
	rfree(dev);
	return NULL;
}

static void rtnr_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "rtnr_free()");

	comp_data_blob_handler_free(cd->model_handler);

	RTKMA_API_Context_Free((void *)&cd->rtk_agl);
	free_RtkMemoryPool();

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int rtnr_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int ret;

	comp_info(dev, "rtnr_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "rtnr_params() error: comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

static int rtnr_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int max_size)
{
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "rtnr_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		break;
	default:
		comp_err(dev, "rtnr_cmd_get_data() error: invalid command %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int rtnr_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "rtnr_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		break;
	default:
		comp_err(dev, "rtnr_cmd_set_data() error: invalid command %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int32_t rtnr_cmd_get_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	int32_t ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "rtnr_cmd_get_value(), SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
			 cdata->comp_id);
		break;
	default:
		comp_err(dev, "rtnr_cmd_get_value() error: invalid cdata->cmd %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int32_t rtnr_cmd_set_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	int32_t ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "rtnr_cmd_set_value(), SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
			 cdata->comp_id);
		break;

	default:
		comp_err(dev, "rtnr_cmd_set_value() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int rtnr_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	comp_info(dev, "rtnr_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = rtnr_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = rtnr_cmd_get_data(dev, cdata, max_data_size);
		break;
	case COMP_CMD_SET_VALUE:
		ret = rtnr_cmd_set_value(dev, cdata);
		break;
	case COMP_CMD_GET_VALUE:
		ret = rtnr_cmd_get_value(dev, cdata);
		break;
	default:
		comp_err(dev, "rtnr_cmd() error: invalid command");
		return -EINVAL;
	}

	return ret;
}

static int rtnr_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "rtnr_trigger() cmd: %d", cmd);

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int rtnr_copy(struct comp_dev *dev)
{
	struct list_item *blist;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	int frames;
	int num_blocks;
	int sink_bytes;
	int source_bytes;
	const struct audio_stream *sources_stream[RTNR_MAX_SOURCES] = { NULL };
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sources[RTNR_MAX_SOURCES] = { NULL };
	uint32_t flags = 0;
	int num_sources = 0;

	comp_dbg(dev, "rtnr_copy()");

	/* align source streams with their respective configurations */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		buffer_lock(source, &flags);
		if (source->source->state == dev->state) {
			if (num_sources < RTNR_MAX_SOURCES) {
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

	/* put empty data into output queue*/
	int nch = sources_stream[0]->channels;
	/* RTKMA uses 32 bits internal storage */
	int copy_size = BLOCKLENGTH * nch * sizeof(int32_t);

	RTKMA_API_First_Copy((void *)&cd->rtk_agl, copy_size);

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	buffer_lock(sink, &flags);
	frames = audio_stream_avail_frames(sources_stream[0], &sink->stream);
	buffer_unlock(sources[0], flags);

	/* Process integer multiple of RTNR internal block length */
	num_blocks = frames / RTNR_BLK_LENGTH;
	frames = num_blocks * RTNR_BLK_LENGTH;

	for (int i = 0; i < num_sources; ++i)
		comp_dbg(dev, "rtnr_copy() sources[%d]->id: %d, frames = %d",
				i, sources[i]->id, frames);

	if (frames) {
		source_bytes = frames * audio_stream_frame_bytes(sources_stream[0]);
		sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);

		buffer_invalidate(sources[0], source_bytes);

		/* Run processing function */
		cd->rtnr_func(dev, sources_stream, &sink->stream, frames);

		buffer_writeback(sink, sink_bytes);

		/*real process function of rtk ma*/
		/* RTKMA uses 32 bits internal storage */
		copy_size = BLOCKLENGTH * MicNum * sizeof(int32_t);
		RTKMA_API_Process((void *)&cd->rtk_agl, copy_size, 0);

		/* Track consume and produce */
		comp_update_buffer_consume(sources[0], source_bytes);
		comp_update_buffer_produce(sink, sink_bytes);
	}


	return 0;
}

static int rtnr_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;
	int ret;

	comp_info(dev, "rtnr_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Get sink data format */
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	cd->sink_format = sinkb->stream.frame_fmt;

	/* Check source and sink PCM format and get copy function */
	comp_info(dev, "rtnr_prepare(), sink_format=%d", cd->sink_format);
	cd->rtnr_func = rtnr_find_func(cd->sink_format);
	if (!cd->rtnr_func) {
		comp_err(dev, "rtnr_prepare(): No suitable processing function found.");
		goto err;
	}

	/* Clear in/out buffers */
	RTKMA_API_Prepare((void *)&cd->rtk_agl);

	/* Initialize RTNR */
	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int rtnr_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	comp_info(dev, "rtnr_reset()");
	cd->rtnr_func = NULL;
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_rtnr = {
	.uid = SOF_RT_UUID(rtnr_uuid),
	.tctx = &rtnr_tr,
	.ops = {
		.create = rtnr_new,
		.free = rtnr_free,
		.params = rtnr_params,
		.cmd = rtnr_cmd,
		.trigger = rtnr_trigger,
		.copy = rtnr_copy,
		.prepare = rtnr_prepare,
		.reset = rtnr_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_rtnr_info = {
	.drv = &comp_rtnr,
};

UT_STATIC void sys_comp_rtnr_init(void)
{
	comp_register(platform_shared_get(&comp_rtnr_info, sizeof(comp_rtnr_info)));
}


DECLARE_MODULE(sys_comp_rtnr_init);

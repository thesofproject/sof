/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Realtek Semiconductor Corp. All rights reserved.
 *
 * Author: Ming Jen Tai <mingjen_tai@realtek.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/rtnr/rtnr.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/string.h>
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

#define MicNum 2
#define SpkNum  2

#define RTNR_BLK_LENGTH			4 /* Must be power of 2 */
#define RTNR_BLK_LENGTH_MASK	(RTNR_BLK_LENGTH - 1)

/* RTNR configuration & data */
#define SOF_RTNR_CONFIG 0
#define SOF_RTNR_DATA 1

/* ID for RTNR data */
#define RTNR_DATA_ID_PRESET 12345678


static const struct comp_driver comp_rtnr;

/** \brief RTNR processing functions map item. */
struct rtnr_func_map {
	enum sof_ipc_frame fmt; /**< source frame format */
	rtnr_func func; /**< processing function */
};

LOG_MODULE_REGISTER(rtnr, CONFIG_SOF_LOG_LEVEL);

/* UUID 5c7ca334-e15d-11eb-ba80-0242ac130004 */
DECLARE_SOF_RT_UUID("rtnr", rtnr_uuid, 0x5c7ca334, 0xe15d, 0x11eb, 0xba, 0x80,
		    0x02, 0x42, 0xac, 0x13, 0x00, 0x04);

DECLARE_TR_CTX(rtnr_tr, SOF_UUID(rtnr_uuid), LOG_LEVEL_INFO);

/* Generic processing */

/* Static functions */
static int rtnr_set_config_bytes(struct comp_dev *dev,
				 const unsigned char *data, uint32_t size);

/* Called by the processing library for debugging purpose */
void rtnr_printf(int a, int b, int c, int d, int e)
{
	switch (a) {
	case 0xa:
		comp_cl_info(&comp_rtnr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
					b, c, d, e);
		break;

	case 0xb:
		comp_cl_info(&comp_rtnr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
					b, c, d, e);
		break;

	case 0xc:
		comp_cl_warn(&comp_rtnr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
					b, c, d, e);
		break;

	case 0xd:
		comp_cl_dbg(&comp_rtnr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
					b, c, d, e);
		break;

	case 0xe:
		comp_cl_err(&comp_rtnr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
					b, c, d, e);
		break;

	default:
		break;
	}
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

static void rtnr_s16_default(struct comp_dev *dev, struct audio_stream_rtnr **sources,
							struct audio_stream_rtnr *sink, int frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	RTKMA_API_S16_Default(cd->rtk_agl, sources, sink, frames,
						0, 0, 0,
						0, 0);
}

#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE

static void rtnr_s24_default(struct comp_dev *dev, struct audio_stream_rtnr **sources,
							struct audio_stream_rtnr *sink, int frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	RTKMA_API_S24_Default(cd->rtk_agl, sources, sink, frames,
						0, 0, 0,
						0, 0);
}

#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE

static void rtnr_s32_default(struct comp_dev *dev, struct audio_stream_rtnr **sources,
							struct audio_stream_rtnr *sink, int frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	RTKMA_API_S32_Default(cd->rtk_agl, sources, sink, frames,
						0, 0, 0,
						0, 0);
}

#endif /* CONFIG_FORMAT_S32LE */

/* Processing functions table */
/*
 *	These functions copy data from source stream to internal queue before
 *	processing, and output data from inter queue to sink stream after
 *	processing.
 */
const struct rtnr_func_map rtnr_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, rtnr_s16_default },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, rtnr_s24_default },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, rtnr_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t rtnr_fncount = ARRAY_SIZE(rtnr_fnmap);

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

static inline void rtnr_set_process_sample_rate(struct comp_dev *dev, uint32_t sample_rate)
{
	comp_dbg(dev, "rtnr_set_process_sample_rate()");
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->process_sample_rate = sample_rate;
}

static int32_t rtnr_check_config_validity(struct comp_dev *dev,
									    struct comp_data *cd)
{
	comp_dbg(dev, "rtnr_check_config_validity() sample_rate:%d enabled: %d",
		cd->config.params.sample_rate, cd->config.params.enabled);

	if ((cd->config.params.sample_rate != 48000) &&
		(cd->config.params.sample_rate != 16000)) {
		comp_err(dev, "rtnr_check_config_validity() invalid sample_rate:%d",
			cd->config.params.sample_rate);
		return -EINVAL;
	}

	rtnr_set_process_sample_rate(dev, cd->config.params.sample_rate);

	return 0;
}

static struct comp_dev *rtnr_new(const struct comp_driver *drv,
				 const struct comp_ipc_config *config,
				 const void *spec)
{
	const struct ipc_config_process *ipc_rtnr = spec;
	struct comp_dev *dev;
	struct comp_data *cd;
	size_t bs = ipc_rtnr->size;
	int ret;

	comp_cl_info(&comp_rtnr, "rtnr_new()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_RTNR_MAX_SIZE) {
		comp_cl_err(&comp_rtnr, "rtnr_new(), error: configuration blob size = %u > %d",
					bs, SOF_RTNR_MAX_SIZE);
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto err;

	comp_set_drvdata(dev, cd);

	cd->process_enable = true;

	/* Handler for component data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_rtnr, "rtnr_new(): comp_data_blob_handler_new() failed.");
		goto cd_fail;
	}

	/* Get initial configuration from topology */
	ret = rtnr_set_config_bytes(dev, ipc_rtnr->data, ipc_rtnr->size);
	if (ret < 0) {
		comp_cl_err(&comp_rtnr, "rtnr_new(): failed setting initial config");
		goto cd_fail;
	}

	/* Component defaults */
	cd->source_channel = 0;

	/* check validity of initial config */
	ret = rtnr_check_config_validity(dev, cd);
	if (ret < 0) {
		comp_cl_err(&comp_rtnr, "rtnr_new(): rtnr_check_config_validity() failed.");
		goto cd_fail;
	}

	cd->rtk_agl = RTKMA_API_Context_Create(cd->process_sample_rate);
	if (cd->rtk_agl == 0) {
		comp_cl_err(&comp_rtnr, "rtnr_new(): RTKMA_API_Context_Create failed.");
		goto cd_fail;
	}
	comp_cl_info(&comp_rtnr, "rtnr_new(): RTKMA_API_Context_Create succeeded.");

	/* comp_is_new_data_blob_available always returns false for the first
	 * control write with non-empty config. The first non-empty write may
	 * happen after prepare (e.g. during copy). Default to true so that
	 * copy keeps checking until a non-empty config is applied.
	 */
	cd->reconfigure = true;

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

	RTKMA_API_Context_Free(cd->rtk_agl);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int rtnr_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb, *sourceb;
	bool channels_valid;

	comp_info(dev, "rtnr_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "rtnr_params() error: comp_verify_params() failed.");
		return ret;
	}

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
							sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
							source_list);

	/* set source/sink_frames/rate */
	cd->source_rate = audio_stream_get_rate(&sourceb->stream);
	cd->sink_rate = audio_stream_get_rate(&sinkb->stream);
	cd->sources_stream[0].rate = audio_stream_get_rate(&sourceb->stream);
	cd->sink_stream.rate = audio_stream_get_rate(&sinkb->stream);
	channels_valid = audio_stream_get_channels(&sourceb->stream) ==
		audio_stream_get_channels(&sinkb->stream);

	if (!cd->sink_rate) {
		comp_err(dev, "rtnr_nr_params(), zero sink rate");
		return -EINVAL;
	}

	/* Currently support 16kHz sample rate only. */
	switch (cd->source_rate) {
	case 16000:
		comp_info(dev, "rtnr_params(), sample rate = 16000 kHz");
		break;
	case 48000:
		comp_info(dev, "rtnr_params(), sample rate = 48000 kHz");
		break;
	default:
		comp_err(dev, "rtnr_nr_params(), invalid sample rate(%d kHz)",
			 cd->source_rate);
		return -EINVAL;
	}

	if (!channels_valid) {
		comp_err(dev, "rtnr_params(), source/sink stream must have same channels");
		return -EINVAL;
	}

	/* set source/sink stream channels */
	cd->sources_stream[0].channels = audio_stream_get_channels(&sourceb->stream);
	cd->sink_stream.channels = audio_stream_get_channels(&sinkb->stream);

	/* set source/sink stream overrun/underrun permitted */
	cd->sources_stream[0].overrun_permitted = audio_stream_get_overrun(&sourceb->stream);
	cd->sink_stream.overrun_permitted = audio_stream_get_overrun(&sinkb->stream);
	cd->sources_stream[0].underrun_permitted = audio_stream_get_underrun(&sourceb->stream);
	cd->sink_stream.underrun_permitted = audio_stream_get_underrun(&sinkb->stream);

	return 0;
}

static int rtnr_get_comp_config(struct comp_data *cd, struct sof_ipc_ctrl_data *cdata,
				int max_data_size)
{
	int ret;

	if (sizeof(cd->config) > max_data_size)
		return -EINVAL;

	ret = memcpy_s(cdata->data->data, max_data_size, &cd->config, sizeof(cd->config));
	if (ret)
		return ret;

	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = sizeof(cd->config);
	return 0;
}

static int rtnr_get_comp_data(struct comp_data *cd, struct sof_ipc_ctrl_data *cdata,
				    int max_data_size)
{
	uint8_t *config;
	size_t size;
	int ret;

	config = comp_get_data_blob(cd->model_handler, &size, NULL);

	if (size > max_data_size || size < 0)
		return -EINVAL;

	if (size > 0) {
		ret = memcpy_s(cdata->data->data,
			       max_data_size,
			       config,
			       size);
		comp_cl_info(&comp_rtnr, "rtnr_get_comp_data(): size= %d, ret = %d",
			     size, ret);
		if (ret)
			return ret;
	}

	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = size;

	return 0;
}

static int rtnr_get_bin_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
				  int max_data_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (!cd)
		return -ENODEV;

	comp_err(dev, "rtnr_get_bin_data(): type = %u, index = %u, size = %d",
		 cdata->data->type, cdata->msg_index, cdata->num_elems);

	switch (cdata->data->type) {
	case SOF_RTNR_CONFIG:
		comp_err(dev, "rtnr_get_bin_data(): SOF_RTNR_CONFIG");
		return rtnr_get_comp_config(cd, cdata, max_data_size);
	case SOF_RTNR_DATA:
		comp_err(dev, "rtnr_get_bin_data(): SOF_RTNR_DATA");
		return rtnr_get_comp_data(cd, cdata, max_data_size);
	default:
		comp_err(dev, "rtnr_get_bin_data(): unknown binary data type");
		return -EINVAL;
	}
}

static int rtnr_cmd_get_data(struct comp_dev *dev,
						struct sof_ipc_ctrl_data *cdata, int max_size)
{
	int ret = 0;

	comp_dbg(dev, "rtnr_cmd_get_data(), SOF_CTRL_CMD_BINARY");

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return rtnr_get_bin_data(dev, cdata, max_size);
	default:
		comp_err(dev, "rtnr_cmd_get_data() error: invalid command %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int rtnr_reconfigure(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint8_t *config;
	size_t size;

	comp_dbg(dev, "rtnr_reconfigure()");

	if (!comp_is_current_data_blob_valid(cd->model_handler) &&
	    !comp_is_new_data_blob_available(cd->model_handler)) {
		/*
		 * The data blob hasn't been available once so far.
		 *
		 * This looks redundant since the same check will be done in
		 * comp_get_data_blob() below. But without this early return,
		 * hundreds of warn message lines are produced per second by
		 * comp_get_data_blob() calls until the data blob is arrived.
		 */
		return 0;
	}

	config = comp_get_data_blob(cd->model_handler, &size, NULL);
	comp_dbg(dev, "rtnr_reconfigure() size: %d", size);

	if (size == 0) {
		/* No data to be handled */
		return 0;
	}

	if (!config) {
		comp_err(dev, "rtnr_reconfigure(): Config not set");
		return -EINVAL;
	}

	comp_info(dev, "rtnr_reconfigure(): New data applied %p (%zu bytes)",
		  config, size);

	cd->reconfigure = false;

	RTKMA_API_Set(cd->rtk_agl, config, size, RTNR_DATA_ID_PRESET);

	return 0;
}

static int rtnr_set_config_bytes(struct comp_dev *dev,
				 const unsigned char *data, uint32_t size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	/*
	 * The received data could be the combined blob of the control
	 * widgets defined in the topology, or the config received by
	 * SOF_CTRL_CMD_BINARY. In either case we just have to check if
	 * the whole config data is received.
	 */
	if (size < sizeof(cd->config)) {
		comp_err(dev, "rtnr_set_config_data(): invalid size %u",
			 size);
		return -EINVAL;
	}

	ret = memcpy_s(&cd->config,
		       sizeof(cd->config),
		       data,
		       sizeof(cd->config));

	comp_info(dev,
		  "rtnr_set_config_data(): sample_rate = %u, enabled=%d",
		  cd->config.params.sample_rate,
		  cd->config.params.enabled);

	return ret;
}

static int rtnr_set_bin_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	assert(cd);
	comp_dbg(dev,
		 "rtnr_set_bin_data(): type = %u", cdata->data->type);

	if (dev->state < COMP_STATE_READY) {
		comp_err(dev, "rtnr_set_bin_data(): driver in init!");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_RTNR_CONFIG:
		return rtnr_set_config_bytes(dev,
					     (unsigned char *)cdata->data->data,
					     cdata->data->size);
	case SOF_RTNR_DATA:
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		if (ret)
			return ret;
		/* Accept the new blob immediately so that userspace can write
		 * the control in quick succession without error.
		 * This ensures the last successful control write from userspace
		 * before prepare/copy is applied.
		 * The config blob is not referenced after reconfigure() returns
		 * so it is safe to call comp_get_data_blob here which frees the
		 * old blob. This assumes cmd() and prepare()/copy() cannot run
		 * concurrently which is the case when there is no preemption.
		 */
		if (comp_is_new_data_blob_available(cd->model_handler)) {
			comp_dbg(dev, "rtnr_set_bin_data(), new data blob available");
			comp_get_data_blob(cd->model_handler, NULL, NULL);
			cd->reconfigure = true;
		}
		break;
	default:
		comp_err(dev, "rtnr_set_bin_data(): unknown binary data type");
		return -EINVAL;
	}

	return ret;
}

static int rtnr_cmd_set_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	int ret;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "rtnr_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		return rtnr_set_bin_data(dev, cdata);
	default:
		comp_err(dev, "rtnr_cmd_set_data() error: invalid command %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int32_t rtnr_cmd_get_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t j;
	int32_t ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->process_enable;
			comp_info(dev, "rtnr_cmd_get_value(), channel = %u, value = %u",
					cdata->chanv[j].channel,
					cdata->chanv[j].value);
		}
		break;
	default:
		comp_err(dev, "rtnr_cmd_get_value() error: invalid cdata->cmd %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int32_t rtnr_set_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	uint32_t val = 0;
	int32_t j;
	struct comp_data *cd = comp_get_drvdata(dev);

	for (j = 0; j < cdata->num_elems; j++) {
		val |= cdata->chanv[j].value;
		comp_info(dev, "rtnr_set_value(), value = %u", val);
	}

	if (val) {
		comp_info(dev, "rtnr_set_value(): enabled");
		cd->process_enable = true;
	} else {
		comp_info(dev, "rtnr_set_value(): passthrough");
		cd->process_enable = false;
	}

	return 0;
}

static int32_t rtnr_cmd_set_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	int32_t ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "rtnr_cmd_set_value(), SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
				cdata->comp_id);
		ret = rtnr_set_value(dev, cdata);
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
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
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

static void rtnr_copy_from_sof_stream(struct audio_stream_rtnr *dst,
				      struct audio_stream *src)
{

	dst->size = audio_stream_get_size(src);
	dst->avail = audio_stream_get_avail(src);
	dst->free = audio_stream_get_free(src);
	dst->w_ptr = audio_stream_get_wptr(src);
	dst->r_ptr = audio_stream_get_rptr(src);
	dst->addr = audio_stream_get_addr(src);
	dst->end_addr = audio_stream_get_end_addr(src);
}

static void rtnr_copy_to_sof_stream(struct audio_stream *dst,
				    struct audio_stream_rtnr *src)
{
	audio_stream_set_size(dst, src->size);
	audio_stream_set_avail(dst, src->avail);
	audio_stream_set_free(dst, src->free);
	audio_stream_set_wptr(dst, src->w_ptr);
	audio_stream_set_rptr(dst, src->r_ptr);
	audio_stream_set_addr(dst, src->addr);
	audio_stream_set_end_addr(dst, src->end_addr);
}

/* copy and process stream data from source to sink buffers */
static int rtnr_copy(struct comp_dev *dev)
{
	struct comp_buffer *sink;
	struct comp_buffer *source;
	int frames;
	int sink_bytes;
	int source_bytes;
	struct comp_copy_limits cl;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct audio_stream_rtnr *sources_stream[RTNR_MAX_SOURCES];
	struct audio_stream_rtnr *sink_stream = &cd->sink_stream;
	int32_t i;
	int ret;

	if (cd->reconfigure) {
		ret = rtnr_reconfigure(dev);
		if (ret)
			return ret;
	}

	for (i = 0; i < RTNR_MAX_SOURCES; ++i)
		sources_stream[i] = &cd->sources_stream[i];

	comp_dbg(dev, "rtnr_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);

	/* put empty data into output queue*/
	RTKMA_API_First_Copy(cd->rtk_agl, cd->source_rate,
			     audio_stream_get_channels(&source->stream));

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	if (cd->process_enable) {
		frames = audio_stream_avail_frames(&source->stream, &sink->stream);

		/* Process integer multiple of RTNR internal block length */
		frames = frames & ~RTNR_BLK_LENGTH_MASK;

		comp_dbg(dev, "rtnr_copy() source->id: %d, frames = %d", source->id, frames);

		if (frames) {
			source_bytes = frames * audio_stream_frame_bytes(&source->stream);
			sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);

			buffer_stream_invalidate(source, source_bytes);

			/* Run processing function */

			/* copy required data from sof audio stream to RTNR audio stream */
			rtnr_copy_from_sof_stream(sources_stream[0], &source->stream);
			rtnr_copy_from_sof_stream(sink_stream, &sink->stream);

			/*
			 * Processing function uses an array of pointers to source streams
			 * as parameter.
			 */
			cd->rtnr_func(dev, (struct audio_stream_rtnr **)&sources_stream,
						sink_stream, frames);

			/*
			 * real process function of rtnr, consume/produce data from internal queue
			 * instead of component buffer
			 */
			RTKMA_API_Process(cd->rtk_agl, 0, cd->source_rate, MicNum);

			/* copy required data from RTNR audio stream to sof audio stream */
			rtnr_copy_to_sof_stream(&source->stream, sources_stream[0]);
			rtnr_copy_to_sof_stream(&sink->stream, sink_stream);

			buffer_stream_writeback(sink, sink_bytes);

			/* Track consume and produce */
			comp_update_buffer_consume(source, source_bytes);
			comp_update_buffer_produce(sink, sink_bytes);
		}
	} else {
		comp_dbg(dev, "rtnr_copy() passthrough");

		/* Get source, sink, number of frames etc. to process. */
		comp_get_copy_limits(source, sink, &cl);

		buffer_stream_invalidate(source, cl.source_bytes);

		audio_stream_copy(&source->stream, 0, &sink->stream, 0,
				audio_stream_get_channels(&source->stream) * cl.frames);

		buffer_stream_writeback(sink, cl.sink_bytes);
		comp_update_buffer_consume(source, cl.source_bytes);
		comp_update_buffer_produce(sink, cl.sink_bytes);
	}

	return 0;
}

static int rtnr_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;
	struct comp_buffer *sink_c;
	int ret;

	comp_dbg(dev, "rtnr_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Check config */
	ret = rtnr_check_config_validity(dev, cd);
	if (ret < 0) {
		comp_cl_err(&comp_rtnr, "rtnr_prepare(): rtnr_check_config_validity() failed.");
		goto err;
	}

	/* Initialize RTNR */

	/* Get sink data format */
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	cd->sink_format = audio_stream_get_frm_fmt(&sinkb->stream);
	cd->sink_stream.frame_fmt = audio_stream_get_frm_fmt(&sinkb->stream);

	/* Check source and sink PCM format and get processing function */
	comp_info(dev, "rtnr_prepare(), sink_format=%d", cd->sink_format);
	cd->rtnr_func = rtnr_find_func(cd->sink_format);
	if (!cd->rtnr_func) {
		comp_err(dev, "rtnr_prepare(): No suitable processing function found.");
		goto err;
	}

	/* Clear in/out buffers */
	RTKMA_API_Prepare(cd->rtk_agl);

	/* Blobs sent during COMP_STATE_READY is assigned to blob_handler->data
	 * directly, so comp_is_new_data_blob_available always returns false.
	 */
	return rtnr_reconfigure(dev);

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int rtnr_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "rtnr_reset()");

	cd->sink_format = 0;
	cd->rtnr_func = NULL;
	cd->source_rate = 0;
	cd->sink_rate = 0;

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
SOF_MODULE_INIT(rtnr, sys_comp_rtnr_init);

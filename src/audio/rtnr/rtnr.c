/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Realtek Semiconductor Corp. All rights reserved.
 *
 * Author: Ming Jen Tai <mingjen_tai@realtek.com>
 */

#include <sof/audio/module_adapter/module/generic.h>
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

#if CONFIG_IPC_MAJOR_4
#include <ipc4/header.h>
#endif

#define MicNum 2
#define SpkNum  2

#define RTNR_BLK_LENGTH			4 /* Must be power of 2 */
#define RTNR_BLK_LENGTH_MASK	(RTNR_BLK_LENGTH - 1)

/* RTNR configuration & data */
#define SOF_RTNR_CONFIG 0
#define SOF_RTNR_DATA 1

/* ID for RTNR data */
#define RTNR_DATA_ID_PRESET 12345678

/** \brief RTNR processing functions map item. */
struct rtnr_func_map {
	enum sof_ipc_frame fmt; /**< source frame format */
	rtnr_func func; /**< processing function */
};

LOG_MODULE_REGISTER(rtnr, CONFIG_SOF_LOG_LEVEL);

/* UUID 5c7ca334-e15d-11eb-ba80-0242ac130004 */
SOF_DEFINE_REG_UUID(rtnr);

DECLARE_TR_CTX(rtnr_tr, SOF_UUID(rtnr_uuid), LOG_LEVEL_INFO);

/* Generic processing */

/* Static functions */
static int rtnr_set_config_bytes(struct processing_module *mod,
				 const unsigned char *data, uint32_t size);

/* Called by the processing library for debugging purpose */
void rtnr_printf(int a, int b, int c, int d, int e)
{
	switch (a) {
	case 0xa:
		tr_info(&rtnr_tr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
			b, c, d, e);
		break;

	case 0xb:
		tr_info(&rtnr_tr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
			b, c, d, e);
		break;

	case 0xc:
		tr_warn(&rtnr_tr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
			b, c, d, e);
		break;

	case 0xd:
		tr_dbg(&rtnr_tr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
		       b, c, d, e);
		break;

	case 0xe:
		tr_err(&rtnr_tr, "rtnr_printf 1st=%08x, 2nd=%08x, 3rd=%08x, 4st=%08x",
		       b, c, d, e);
		break;

	default:
		break;
	}
}

void *rtk_rballoc(unsigned int flags, unsigned int caps, unsigned int bytes)
{
	return rballoc(flags, bytes);
}

void rtk_rfree(void *ptr)
{
	rfree(ptr);
}

#if CONFIG_FORMAT_S16LE

static void rtnr_s16_default(struct processing_module *mod, struct audio_stream_rtnr **sources,
			     struct audio_stream_rtnr *sink, int frames)
{
	struct comp_data *cd = module_get_private_data(mod);

	RTKMA_API_S16_Default(cd->rtk_agl, sources, sink, frames,
						0, 0, 0,
						0, 0);
}

#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE

static void rtnr_s24_default(struct processing_module *mod, struct audio_stream_rtnr **sources,
			     struct audio_stream_rtnr *sink, int frames)
{
	struct comp_data *cd = module_get_private_data(mod);

	RTKMA_API_S24_Default(cd->rtk_agl, sources, sink, frames,
						0, 0, 0,
						0, 0);
}

#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE

static void rtnr_s32_default(struct processing_module *mod, struct audio_stream_rtnr **sources,
			     struct audio_stream_rtnr *sink, int frames)
{
	struct comp_data *cd = module_get_private_data(mod);

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

static inline void rtnr_set_process_sample_rate(struct processing_module *mod, uint32_t sample_rate)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "rtnr_set_process_sample_rate()");
	cd->process_sample_rate = sample_rate;
}

static int32_t rtnr_check_config_validity(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "rtnr_check_config_validity() sample_rate:%d enabled: %d",
		cd->config.params.sample_rate, cd->config.params.enabled);

	if ((cd->config.params.sample_rate != 48000) &&
		(cd->config.params.sample_rate != 16000)) {
		comp_err(dev, "rtnr_check_config_validity() invalid sample_rate:%d",
			cd->config.params.sample_rate);
		return -EINVAL;
	}

	rtnr_set_process_sample_rate(mod, cd->config.params.sample_rate);

	return 0;
}

static int rtnr_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *ipc_rtnr = &mod_data->cfg;
	struct comp_data *cd;
	size_t bs = ipc_rtnr->size;
	int ret;

	comp_info(dev, "rtnr_new()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_RTNR_MAX_SIZE) {
		comp_err(dev, "rtnr_new(), error: configuration blob size = %u > %d",
			 bs, SOF_RTNR_MAX_SIZE);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	mod_data->private = cd;

	cd->process_enable = true;

	/* Handler for component data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "rtnr_new(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto cd_fail;
	}

	ret = comp_init_data_blob(cd->model_handler, bs, ipc_rtnr->data);
	if (ret < 0) {
		comp_err(dev, "rtnr_init(): comp_init_data_blob() failed with error: %d", ret);
		goto cd_fail;
	}

	/* Component defaults */
	cd->source_channel = 0;

	cd->rtk_agl = RTKMA_API_Context_Create(cd->process_sample_rate);
	if (cd->rtk_agl == 0) {
		comp_err(dev, "rtnr_new(): RTKMA_API_Context_Create failed.");
		ret = -EINVAL;
		goto cd_fail;
	}
	comp_info(dev, "rtnr_new(): RTKMA_API_Context_Create succeeded.");

	/* comp_is_new_data_blob_available always returns false for the first
	 * control write with non-empty config. The first non-empty write may
	 * happen after prepare (e.g. during copy). Default to true so that
	 * copy keeps checking until a non-empty config is applied.
	 */
	cd->reconfigure = true;

	/* Done. */
	return 0;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	return ret;
}

static int rtnr_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "rtnr_free()");

	comp_data_blob_handler_free(cd->model_handler);

	RTKMA_API_Context_Free(cd->rtk_agl);

	rfree(cd);
	return 0;
}

/* Check component audio stream parameters */
static int rtnr_check_params(struct processing_module *mod, struct audio_stream *source,
			     struct audio_stream *sink)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	bool channels_valid;

	comp_info(dev, "rtnr_check_params()");

	/* set source/sink_frames/rate */
	cd->source_rate = audio_stream_get_rate(source);
	cd->sink_rate = audio_stream_get_rate(sink);
	cd->sources_stream[0].rate = audio_stream_get_rate(source);
	cd->sink_stream.rate = audio_stream_get_rate(sink);
	channels_valid = audio_stream_get_channels(source) == audio_stream_get_channels(sink);

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
	cd->sources_stream[0].channels = audio_stream_get_channels(source);
	cd->sink_stream.channels = audio_stream_get_channels(sink);

	/* set source/sink stream overrun/underrun permitted */
	cd->sources_stream[0].overrun_permitted = audio_stream_get_overrun(source);
	cd->sink_stream.overrun_permitted = audio_stream_get_overrun(sink);
	cd->sources_stream[0].underrun_permitted = audio_stream_get_underrun(source);
	cd->sink_stream.underrun_permitted = audio_stream_get_underrun(sink);

	return 0;
}

#if CONFIG_IPC_MAJOR_3
static int rtnr_get_comp_config(struct processing_module *mod, struct sof_ipc_ctrl_data *cdata,
				int max_data_size)
{
	struct comp_data *cd = module_get_private_data(mod);
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

static int rtnr_get_comp_data(struct processing_module *mod, struct sof_ipc_ctrl_data *cdata,
			      int max_data_size)
{
	struct comp_data *cd = module_get_private_data(mod);
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
		comp_info(mod->dev, "rtnr_get_comp_data(): size= %d, ret = %d",
			  size, ret);
		if (ret)
			return ret;
	}

	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = size;

	return 0;
}

static int rtnr_get_bin_data(struct processing_module *mod, struct sof_ipc_ctrl_data *cdata,
			     int max_data_size)
{
	struct comp_dev *dev = mod->dev;

	if (!dev)
		return -ENODEV;

	comp_err(dev, "rtnr_get_bin_data(): type = %u, index = %u, size = %d",
		 cdata->data->type, cdata->msg_index, cdata->num_elems);

	switch (cdata->data->type) {
	case SOF_RTNR_CONFIG:
		comp_err(dev, "rtnr_get_bin_data(): SOF_RTNR_CONFIG");
		return rtnr_get_comp_config(mod, cdata, max_data_size);
	case SOF_RTNR_DATA:
		comp_err(dev, "rtnr_get_bin_data(): SOF_RTNR_DATA");
		return rtnr_get_comp_data(mod, cdata, max_data_size);
	default:
		comp_err(dev, "rtnr_get_bin_data(): unknown binary data type");
		return -EINVAL;
	}
}
#endif

static int rtnr_get_config(struct processing_module *mod,
			   uint32_t param_id, uint32_t *data_offset_size,
			   uint8_t *fragment, size_t fragment_size)
{
	struct comp_dev *dev = mod->dev;

#if CONFIG_IPC_MAJOR_4
	comp_err(dev, "rtnr_get_config(), Not supported, should not happen");
	return -EINVAL;

#elif CONFIG_IPC_MAJOR_3
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	int j;

	comp_dbg(dev, "rtnr_get_config()");

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return rtnr_get_bin_data(mod, cdata, fragment_size);

	case SOF_CTRL_CMD_SWITCH:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->process_enable;
			comp_dbg(dev, "rtnr_cmd_get_value(), channel = %u, value = %u",
				 cdata->chanv[j].channel,
				 cdata->chanv[j].value);
		}
		break;
	default:
		comp_err(dev, "rtnr_cmd_get_data() error: invalid command %d", cdata->cmd);
		return -EINVAL;
	}
#endif /* CONFIG_IPC_MAJOR_3 */

	return 0;
}

static int rtnr_reconfigure(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
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

static int rtnr_set_config_bytes(struct processing_module *mod,
				 const unsigned char *data, uint32_t size)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
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

static int32_t rtnr_set_value(struct processing_module *mod, void *ctl_data)
{
#if CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = ctl_data;
#elif CONFIG_IPC_MAJOR_4
	struct sof_ipc4_control_msg_payload *cdata = ctl_data;
#endif
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	uint32_t val = 0;
	int32_t j;

	for (j = 0; j < cdata->num_elems; j++) {
		val |= cdata->chanv[j].value;
		comp_dbg(dev, "rtnr_set_value(), value = %u", val);
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

static int rtnr_set_config(struct processing_module *mod, uint32_t param_id,
			   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			   const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			   size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

#if CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		if (dev->state < COMP_STATE_READY) {
			comp_err(dev, "rtnr_set_config(): driver in init!");
			return -EBUSY;
		}

		switch (cdata->data->type) {
		case SOF_RTNR_CONFIG:
			return rtnr_set_config_bytes(mod, (unsigned char *)cdata->data->data,
						     cdata->data->size);
		case SOF_RTNR_DATA:
			ret = comp_data_blob_set(cd->model_handler, pos, data_offset_size,
						 fragment, fragment_size);
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
				comp_dbg(dev, "rtnr_set_config(), new data blob available");
				comp_get_data_blob(cd->model_handler, NULL, NULL);
				cd->reconfigure = true;
			}
			return 0;
		}

		comp_err(dev, "rtnr_set_config(): unknown binary data type");
		return -EINVAL;

	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "rtnr_cmd_set_config(), SOF_CTRL_CMD_SWITCH");
		return rtnr_set_value(mod, cdata);
	}

	comp_err(dev, "rtnr_set_config() error: invalid command %d", cdata->cmd);
	return -EINVAL;

#elif CONFIG_IPC_MAJOR_4
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		comp_dbg(dev, "rtnr_set_config(), SOF_IPC4_SWITCH_CONTROL_PARAM_ID");
		return rtnr_set_value(mod, ctl);
	case SOF_RTNR_CONFIG:
		comp_dbg(dev, "rtnr_set_config(), SOF_RTNR_CONFIG");
		if (dev->state < COMP_STATE_READY) {
			comp_err(dev, "rtnr_set_config(): driver in init!");
			return -EBUSY;
		}

		return rtnr_set_config_bytes(mod, fragment, fragment_size);
	case SOF_RTNR_DATA:
		comp_dbg(dev, "rtnr_set_config(), SOF_RTNR_DATA");
		if (dev->state < COMP_STATE_READY) {
			comp_err(dev, "rtnr_set_config(): driver in init!");
			return -EBUSY;
		}

		ret = comp_data_blob_set(cd->model_handler, pos, data_offset_size,
					 fragment, fragment_size);
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
		return 0;
	}

	comp_err(dev, "rtnr_set_config(), error: invalid param_id = %d", param_id);
	return -EINVAL;
#endif
}

void rtnr_copy_from_sof_stream(struct audio_stream_rtnr *dst, struct audio_stream *src)
{

	dst->size = audio_stream_get_size(src);
	dst->avail = audio_stream_get_avail(src);
	dst->free = audio_stream_get_free(src);
	dst->w_ptr = audio_stream_get_wptr(src);
	dst->r_ptr = audio_stream_get_rptr(src);
	dst->addr = audio_stream_get_addr(src);
	dst->end_addr = audio_stream_get_end_addr(src);
}

void rtnr_copy_to_sof_stream(struct audio_stream *dst, struct audio_stream_rtnr *src)
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
static int rtnr_process(struct processing_module *mod,
			struct input_stream_buffer *input_buffers, int num_input_buffers,
			struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	int frames = input_buffers[0].size;
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream_rtnr *sources_stream[RTNR_MAX_SOURCES];
	struct audio_stream_rtnr *sink_stream = &cd->sink_stream;
	int32_t i;
	int ret;

	if (cd->reconfigure) {
		ret = rtnr_reconfigure(mod);
		if (ret)
			return ret;
	}

	for (i = 0; i < RTNR_MAX_SOURCES; ++i)
		sources_stream[i] = &cd->sources_stream[i];

	comp_dbg(dev, "rtnr_copy()");

	/* put empty data into output queue*/
	RTKMA_API_First_Copy(cd->rtk_agl, cd->source_rate, audio_stream_get_channels(source));

	if (!frames)
		return 0;

	comp_dbg(dev, "rtnr_copy() frames = %d", frames);
	if (cd->process_enable) {
		/* Run processing function */

		/* copy required data from sof audio stream to RTNR audio stream */
		rtnr_copy_from_sof_stream(sources_stream[0], source);
		rtnr_copy_from_sof_stream(sink_stream, sink);

		/*
		 * Processing function uses an array of pointers to source streams
		 * as parameter.
		 */
		cd->rtnr_func(mod, sources_stream, sink_stream, frames);

		/*
		 * real process function of rtnr, consume/produce data from internal queue
		 * instead of component buffer
		 */
		RTKMA_API_Process(cd->rtk_agl, 0, cd->source_rate, MicNum);

		/* copy required data from RTNR audio stream to sof audio stream */
		rtnr_copy_to_sof_stream(source, sources_stream[0]);
		rtnr_copy_to_sof_stream(sink, sink_stream);

	} else {
		comp_dbg(dev, "rtnr_copy() passthrough");

		audio_stream_copy(source, 0, sink, 0, frames * audio_stream_get_channels(source));
	}

	/* Track consume and produce */
	module_update_buffer_position(&input_buffers[0], &output_buffers[0], frames);
	return 0;
}

#if CONFIG_IPC_MAJOR_4
static void rtnr_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct comp_dev *dev = mod->dev;

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);

	/* The caller has checked validity of source and sink buffers */

	sourceb = comp_dev_get_first_data_producer(dev);
	ipc4_update_buffer_format(sourceb, &mod->priv.cfg.base_cfg.audio_fmt);

	sinkb = comp_dev_get_first_data_consumer(dev);
	ipc4_update_buffer_format(sinkb, &mod->priv.cfg.base_cfg.audio_fmt);
}
#endif

static int rtnr_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb, *sinkb;
	int ret;

	comp_dbg(dev, "rtnr_prepare()");

	sinkb = comp_dev_get_first_data_consumer(dev);
	sourceb = comp_dev_get_first_data_producer(dev);
	if (!sourceb || !sinkb) {
		comp_err(dev, "no source or sink buffer");
		return -ENOTCONN;
	}

#if CONFIG_IPC_MAJOR_4
	rtnr_params(mod);
#endif

	/* Check config */
	ret = rtnr_check_config_validity(mod);
	if (ret < 0) {
		comp_err(dev, "rtnr_prepare(): rtnr_check_config_validity() failed.");
		goto err;
	}

	/* Initialize RTNR */

	/* Get sink data format */
	cd->sink_format = audio_stream_get_frm_fmt(&sinkb->stream);
	cd->sink_stream.frame_fmt = audio_stream_get_frm_fmt(&sinkb->stream);
	ret = rtnr_check_params(mod, &sourceb->stream, &sinkb->stream);
	if (ret)
		goto err;

	/* Check source and sink PCM format and get processing function */
	comp_info(dev, "rtnr_prepare(), sink_format=%d", cd->sink_format);
	cd->rtnr_func = rtnr_find_func(cd->sink_format);
	if (!cd->rtnr_func) {
		comp_err(dev, "rtnr_prepare(): No suitable processing function found.");
		ret = -EINVAL;
		goto err;
	}

	/* Clear in/out buffers */
	RTKMA_API_Prepare(cd->rtk_agl);

	/* Blobs sent during COMP_STATE_READY is assigned to blob_handler->data
	 * directly, so comp_is_new_data_blob_available always returns false.
	 */
	return rtnr_reconfigure(mod);

err:
	return ret;
}

static int rtnr_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "rtnr_reset()");

	cd->sink_format = 0;
	cd->rtnr_func = NULL;
	cd->source_rate = 0;
	cd->sink_rate = 0;

	return 0;
}

static const struct module_interface rtnr_interface = {
	.init = rtnr_init,
	.prepare = rtnr_prepare,
	.process_audio_stream = rtnr_process,
	.set_configuration = rtnr_set_config,
	.get_configuration = rtnr_get_config,
	.reset = rtnr_reset,
	.free = rtnr_free
};

#if CONFIG_COMP_RTNR_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(rtnr, &rtnr_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("RTNR", rtnr_llext_entry, 1, SOF_REG_UUID(rtnr), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(rtnr_interface, rtnr_uuid, rtnr_tr);
SOF_MODULE_INIT(rtnr, sys_comp_module_rtnr_interface_init);

#endif

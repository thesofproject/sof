// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * A codec adapter component.
 */

/**
 * \file audio/codec_adapter.c
 * \brief Processing compoent aimed to work with external codec libraries
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/codec_adapter/codec_adapter.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/platform.h>
#include <sof/ut.h>

static const struct comp_driver comp_codec_adapter;

/* d8218443-5ff3-4a4c-b388-6cfe07b956aa */
DECLARE_SOF_RT_UUID("codec_adapter", ca_uuid, 0xd8218443, 0x5ff3, 0x4a4c,
		    0xb3, 0x88, 0x6c, 0xfe, 0x07, 0xb9, 0x56, 0xaa);

DECLARE_TR_CTX(ca_tr, SOF_UUID(ca_uuid), LOG_LEVEL_INFO);

/**
 * \brief Create a codec adapter component.
 * \param[in] drv - component driver pointer.
 * \param[in] comp - component ipc descriptor pointer.
 *
 * \return: a pointer to newly created codec adapter component.
 */
static struct comp_dev *codec_adapter_new(const struct comp_driver *drv,
					  struct sof_ipc_comp *comp)
{
	int ret;
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_process *ipc_codec_adapter =
		(struct sof_ipc_comp_process *)comp;

	comp_cl_info(&comp_codec_adapter, "codec_adapter_new() start");

	if (!drv || !comp) {
		comp_cl_err(&comp_codec_adapter, "codec_adapter_new(), wrong input params! drv = %x comp = %x",
			    (uint32_t)drv, (uint32_t)comp);
		return NULL;
	}

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev) {
		comp_cl_err(&comp_codec_adapter, "codec_adapter_new(), failed to allocate memory for comp_dev");
		return NULL;
	}

	dev->drv = drv;

	ret = memcpy_s(&dev->comp, sizeof(struct sof_ipc_comp_process),
		       comp, sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		comp_cl_err(&comp_codec_adapter, "codec_adapter_new(), failed to allocate memory for comp_data");
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	/* Copy setup config */
	ret = load_setup_config(dev, ipc_codec_adapter->data, ipc_codec_adapter->size);
	if (ret) {
		comp_err(dev, "codec_adapter_new() error %d: config loading has failed.",
			 ret);
		goto err;
	}
	/* Init processing codec */
	ret = codec_init(dev);
	if (ret) {
		comp_err(dev, "codec_adapter_new() %d: codec initialization failed",
			 ret);
		goto err;
	}

	dev->state = COMP_STATE_READY;
	cd->state = PP_STATE_CREATED;

	comp_cl_info(&comp_codec_adapter, "codec_adapter_new() done");
	return dev;
err:
	rfree(cd);
	rfree(dev);
	return NULL;
}

static inline int validate_setup_config(struct ca_config *cfg)
{
	/* TODO: validate codec_adapter setup parameters */
	return 0;
}

/**
 * \brief Load setup config for both codec adapter and codec library.
 * \param[in] dev - codec adapter component device pointer.
 * \param[in] cfg - pointer to the configuration data.
 * \param[in] size - size of config.
 *
 * The setup config comprises of two parts - one contains essential data
 * for the initialization of codec_adapter and follows struct ca_config.
 * Second contains codec specific data needed to setup the codec itself.
 * The latter is send in a TLV format organized by struct codec_param.
 *
 * \return integer representing either:
 *	0 -> success
 *	negative value -> failure.
 */
static int load_setup_config(struct comp_dev *dev, void *cfg, uint32_t size)
{
	int ret;
	void *lib_cfg;
	size_t lib_cfg_size;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "load_setup_config() start.");

	if (!dev) {
		comp_err(dev, "load_setup_config(): no component device.");
		ret = -EINVAL;
		goto end;
	} else if (!cfg || !size) {
		comp_err(dev, "load_setup_config(): no config available cfg: %x, size: %d",
			 (uintptr_t)cfg, size);
		ret = -EINVAL;
		goto end;
	} else if (size <= sizeof(struct ca_config)) {
		comp_err(dev, "load_setup_config(): no codec config available.");
		ret = -EIO;
		goto end;
	}
	/* Copy codec_adapter part */
	ret = memcpy_s(&cd->ca_config, sizeof(cd->ca_config), cfg,
		       sizeof(struct ca_config));
	assert(!ret);
	ret = validate_setup_config(&cd->ca_config);
	if (ret) {
		comp_err(dev, "load_setup_config(): validation of setup config for codec_adapter failed.");
		goto end;
	}
	/* Copy codec specific part */
	lib_cfg = (char *)cfg + sizeof(struct ca_config);
	lib_cfg_size = size - sizeof(struct ca_config);
	ret = codec_load_config(dev, lib_cfg, lib_cfg_size, CODEC_CFG_SETUP);
	if (ret) {
		comp_err(dev, "load_setup_config(): %d: failed to load setup config for codec id %x",
			 ret, cd->ca_config.codec_id);
		goto end;
	}

	comp_dbg(dev, "load_setup_config() done.");
end:
	return ret;
}

/*
 * \brief Prepare a codec adapter component.
 * \param[in] dev - component device pointer.
 *
 * \return integer representing either:
 *	0 - success
 *	value < 0 - failure.
 */
static int codec_adapter_prepare(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "codec_adapter_prepare() start");

	/* Init sink & source buffers */
	cd->ca_sink = list_first_item(&dev->bsink_list, struct comp_buffer,
				      source_list);
	cd->ca_source = list_first_item(&dev->bsource_list, struct comp_buffer,
					sink_list);

	if (!cd->ca_source) {
		comp_err(dev, "codec_adapter_prepare(): source buffer not found");
		return -EINVAL;
	} else if (!cd->ca_sink) {
		comp_err(dev, "codec_adapter_prepare(): sink buffer not found");
		return -EINVAL;
	}

	/* Are we already prepared? */
	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET) {
		comp_warn(dev, "codec_adapter_prepare(): codec_adapter has already been prepared");
		return PPL_STATUS_PATH_STOP;
	}

	/* Prepare codec */
	ret = codec_prepare(dev);
	if (ret) {
		comp_err(dev, "codec_adapter_prepare() error %x: codec prepare failed",
			 ret);

		return -EIO;
	}

	comp_info(dev, "codec_adapter_prepare() done");
	cd->state = PP_STATE_PREPARED;

	return 0;
}

static int codec_adapter_params(struct comp_dev *dev,
				    struct sof_ipc_stream_params *params)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "codec_adapter_params(): comp_verify_params() failed.");
		return ret;
	}

	ret = memcpy_s(&cd->stream_params, sizeof(struct sof_ipc_stream_params),
		       params, sizeof(struct sof_ipc_stream_params));
	assert(!ret);

	cd->period_bytes = params->sample_container_bytes *
			   params->channels * params->rate / 1000;
	return 0;
}

static void
codec_adapter_copy_from_source_to_lib(const struct audio_stream *source,
				      const struct codec_processing_data *cpd,
				      size_t bytes)
{
	/* head_size - available data until end of local buffer */
	uint32_t head_size = MIN(bytes, audio_stream_bytes_without_wrap(source,
				 source->r_ptr));
	/* tail_size - residue data to be copied starting from the beginning
	 * of the buffer
	 */
	uint32_t tail_size = bytes - head_size;
	/* copy head_size to lib buffer */
	memcpy_s(cpd->in_buff, cpd->in_buff_size, source->r_ptr, head_size);
	if (tail_size)
	/* copy reset of the samples after wrap-up */
		memcpy_s((char *)cpd->in_buff + head_size, cpd->in_buff_size,
			 audio_stream_wrap(source,
					   (char *)source->r_ptr + head_size),
					   tail_size);
}

static void
codec_adapter_copy_from_lib_to_sink(const struct codec_processing_data *cpd,
				    const struct audio_stream *sink,
				    size_t bytes)
{
	/* head_size - free space until end of local buffer */
	uint32_t head_size = MIN(bytes, audio_stream_bytes_without_wrap(sink,
				 sink->w_ptr));
	/* tail_size - rest of the bytes that needs to be written
	 * starting from the beginning of the buffer
	 */
	uint32_t tail_size = bytes - head_size;

	/* copy head_size to sink buffer */
	memcpy_s(sink->w_ptr, sink->size, cpd->out_buff, head_size);
	if (tail_size)
	/* copy reset of the samples after wrap-up */
		memcpy_s(audio_stream_wrap(sink,
					   (char *)sink->w_ptr + head_size),
			 sink->size,
			 (char *)cpd->out_buff + head_size, tail_size);
}

static int codec_adapter_copy(struct comp_dev *dev)
{
	int ret = 0;
	uint32_t bytes_to_process, processed = 0;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_data *codec = &cd->codec;
	struct comp_buffer *source = cd->ca_source;
	struct comp_buffer *sink = cd->ca_sink;
	uint32_t codec_buff_size = codec->cpd.in_buff_size;
	struct comp_copy_limits cl;

	comp_get_copy_limits_with_lock(source, sink, &cl);
	bytes_to_process = cl.frames * cl.source_frame_bytes;

	comp_dbg(dev, "codec_adapter_copy() start: codec_buff_size: %d, sink free: %d source avail %d",
		 codec_buff_size, sink->stream.free, source->stream.avail);

	while (bytes_to_process) {
		/* Proceed only if we have enough data to fill the lib buffer
		 * completely. If you don't fill whole buffer
		 * the lib won't process it.
		 */
		if (bytes_to_process < codec_buff_size) {
			comp_dbg(dev, "codec_adapter_copy(): processed %d in this call %d bytes left for next period",
				 processed, bytes_to_process);
			break;
		}

		codec_adapter_copy_from_source_to_lib(&source->stream, &codec->cpd,
						      codec_buff_size);
		buffer_invalidate(source, codec_buff_size);
		codec->cpd.avail = codec_buff_size;
		ret = codec_process(dev);
		if (ret) {
			comp_err(dev, "codec_adapter_copy() error %x: lib processing failed",
				 ret);
			break;
		} else if (codec->cpd.produced == 0) {
			/* skipping as lib has not produced anything */
			comp_err(dev, "codec_adapter_copy() error %x: lib hasn't processed anything",
				 ret);
			break;
		}
		codec_adapter_copy_from_lib_to_sink(&codec->cpd, &sink->stream,
						    codec->cpd.produced);
		buffer_writeback(sink, codec->cpd.produced);

		bytes_to_process -= codec->cpd.produced;
		processed += codec->cpd.produced;
	}

	if (!processed) {
		comp_dbg(dev, "codec_adapter_copy(): nothing processed in this call");
		goto end;
	}

	comp_update_buffer_produce(sink, processed);
	comp_update_buffer_consume(source, processed);
end:
	return ret;
}

static int codec_adapter_set_params(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
				    enum codec_cfg_type type)
{
	int ret;
	char *dst, *src;
	static uint32_t size;
	uint32_t offset;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_data *codec = &cd->codec;

	comp_dbg(dev, "codec_adapter_set_params(): start: num_of_elem %d, elem remain %d msg_index %u",
		 cdata->num_elems, cdata->elems_remaining, cdata->msg_index);

	/* Stage 1 verify input params & allocate memory for the config blob */
	if (cdata->msg_index == 0) {
		size = cdata->num_elems + cdata->elems_remaining;
		/* Check that there is no work-in-progress on previous request */
		if (codec->runtime_params) {
			comp_err(dev, "codec_adapter_set_params() error: busy with previous request");
			ret = -EBUSY;
			goto end;
		} else if (!size) {
			comp_err(dev, "codec_adapter_set_params() error: no configuration size %d",
				 size);
			/* TODO: return -EINVAL. This is temporary until driver fixes its issue */
			ret = 0;
			goto end;
		} else if (size > MAX_BLOB_SIZE) {
			comp_err(dev, "codec_adapter_set_params() error: blob size is too big cfg size %d, allowed %d",
				 size, MAX_BLOB_SIZE);
			ret = -EINVAL;
			goto end;
		} else if (type != CODEC_CFG_SETUP && type != CODEC_CFG_RUNTIME) {
			comp_err(dev, "codec_adapter_set_params() error: unknown config type");
			ret = -EINVAL;
			goto end;
		}

		/* Allocate buffer for new params */
		codec->runtime_params = rballoc(0, SOF_MEM_CAPS_RAM, size);
		if (!codec->runtime_params) {
			comp_err(dev, "codec_adapter_set_params(): space allocation for new params failed");
			ret = -ENOMEM;
			goto end;
		}

		memset(codec->runtime_params, 0, size);
	} else if (!codec->runtime_params) {
		comp_err(dev, "codec_adapter_set_params() error: no memory available for runtime params in consecutive load");
		ret = -EIO;
		goto end;
	}

	offset = size - (cdata->num_elems + cdata->elems_remaining);
	dst = (char *)codec->runtime_params + offset;
	src = (char *)cdata->data->data;

	ret = memcpy_s(dst, size - offset, src, cdata->num_elems);
	assert(!ret);

	/* Config has been copied now we can load & apply it depending on
	 * codec state.
	 */
	if (!cdata->elems_remaining) {
		switch (type) {
		case CODEC_CFG_SETUP:
			ret = load_setup_config(dev, codec->runtime_params, size);
			if (ret) {
				comp_err(dev, "codec_adapter_set_params(): error %d: load of setup config failed.",
					 ret);
			} else {
				comp_dbg(dev, "codec_adapter_set_params() load of setup config done.");
			}

			break;
		case CODEC_CFG_RUNTIME:
			ret = codec_load_config(dev, codec->runtime_params, size,
						CODEC_CFG_RUNTIME);
			if (ret) {
				comp_err(dev, "codec_adapter_set_params() error %d: load of runtime config failed.",
					 ret);
				goto done;
			} else {
				comp_dbg(dev, "codec_adapter_set_params() load of runtime config done.");
			}

			if (cd->state >= PP_STATE_PREPARED) {
				/* We are already prepared so we can apply runtime
				 * config right away.
				 */
				ret = codec_apply_runtime_config(dev);
				if (ret) {
					comp_err(dev, "codec_adapter_set_params() error %x: codec runtime config apply failed",
						 ret);
				}  else {
					comp_dbg(dev, "codec_adapter_set_params() apply of runtime config done.");
				}
			} else {
				cd->codec.r_cfg.avail = true;
			}

			break;
		default:
			comp_err(dev, "codec_adapter_set_params(): error: unknown config type.");
			break;
		}
	} else
		goto end;

done:
	if (codec->runtime_params)
		rfree(codec->runtime_params);
	codec->runtime_params = NULL;
	return ret;
end:
	return ret;
}

static int ca_set_binary_data(struct comp_dev *dev,
			      struct sof_ipc_ctrl_data *cdata)
{
	int ret;

	comp_info(dev, "ca_set_binary_data() start, data type %d",
		  cdata->data->type);

	switch (cdata->data->type) {
	case CODEC_CFG_SETUP:
	case CODEC_CFG_RUNTIME:
		ret = codec_adapter_set_params(dev, cdata, cdata->data->type);
		break;
	default:
		comp_err(dev, "ca_set_binary_data() error: unknown binary data type");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int codec_adapter_ctrl_set_data(struct comp_dev *dev,
				       struct sof_ipc_ctrl_data *cdata)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "codec_adapter_ctrl_set_data() start, state %d, cmd %d",
		  cd->state, cdata->cmd);

	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		comp_err(dev, "codec_adapter_ctrl_set_data(): ABI mismatch!");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_err(dev, "codec_adapter_ctrl_set_data() set enum is not implemented for codec_adapter.");
		ret = -EIO;
		break;
	case SOF_CTRL_CMD_BINARY:
		ret = ca_set_binary_data(dev, cdata);
		break;
	default:
		comp_err(dev, "codec_adapter_ctrl_set_data error: unknown set data command");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* Used to pass standard and bespoke commands (with data) to component */
static int codec_adapter_cmd(struct comp_dev *dev, int cmd, void *data,
			     int max_data_size)
{
	int ret;
	struct sof_ipc_ctrl_data *cdata = data;

	comp_info(dev, "codec_adapter_cmd() %d start", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = codec_adapter_ctrl_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		comp_err(dev, "codec_adapter_cmd() get_data not implemented yet.");
		ret = -ENODATA;
		break;
	default:
		comp_err(dev, "codec_adapter_cmd() error: unknown command");
		ret = -EINVAL;
		break;
	}

	comp_info(dev, "codec_adapter_cmd() done");
	return ret;
}

static int codec_adapter_trigger(struct comp_dev *dev, int cmd)
{
	comp_cl_info(&comp_codec_adapter, "codec_adapter_trigger(): component got trigger cmd %x",
		     cmd);

	return comp_set_state(dev, cmd);
}

static int codec_adapter_reset(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_cl_info(&comp_codec_adapter, "codec_adapter_reset(): resetting");

	ret = codec_reset(dev);
	if (ret) {
		comp_cl_info(&comp_codec_adapter, "codec_adapter_reset(): error %d, codec reset has failed",
			     ret);
	}

	cd->state = PP_STATE_CREATED;

	comp_cl_info(&comp_codec_adapter, "codec_adapter_reset(): done");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static void codec_adapter_free(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_cl_info(&comp_codec_adapter, "codec_adapter_free(): start");

	ret = codec_free(dev);
	if (ret) {
		comp_cl_info(&comp_codec_adapter, "codec_adapter_reset(): error %d, codec reset has failed",
			     ret);
	}
	rfree(cd);
	rfree(dev);

	comp_cl_info(&comp_codec_adapter, "codec_adapter_free(): component memory freed");
}

static const struct comp_driver comp_codec_adapter = {
	.type = SOF_COMP_NONE,
	.uid = SOF_RT_UUID(ca_uuid),
	.tctx = &ca_tr,
	.ops = {
		.create = codec_adapter_new,
		.prepare = codec_adapter_prepare,
		.params = codec_adapter_params,
		.copy = codec_adapter_copy,
		.cmd = codec_adapter_cmd,
		.trigger = codec_adapter_trigger,
		.reset = codec_adapter_reset,
		.free = codec_adapter_free,
	},
};

static SHARED_DATA struct comp_driver_info comp_codec_adapter_info = {
	.drv = &comp_codec_adapter,
};

UT_STATIC void sys_comp_codec_adapter_init(void)
{
	comp_register(platform_shared_get(&comp_codec_adapter_info,
					  sizeof(comp_codec_adapter_info)));
}

DECLARE_MODULE(sys_comp_codec_adapter_init);

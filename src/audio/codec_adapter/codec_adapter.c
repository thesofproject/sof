// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * A codec adapter component.
 */

/**
 * \file
 * \brief Processing compoent aimed to work with external codec libraries
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/codec_adapter/codec_adapter.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/platform.h>
#include <sof/ut.h>

int load_setup_config(struct comp_dev *dev, void *cfg, uint32_t size);
int validate_setup_config(struct ca_config *cfg);

/**
 * \brief Create a codec adapter component.
 * \param[in] drv - component driver pointer.
 * \param[in] config - component ipc descriptor pointer.
 *
 * \return: a pointer to newly created codec adapter component.
 */
struct comp_dev *codec_adapter_new(const struct comp_driver *drv,
				   struct comp_ipc_config *config,
				   struct module_interface *interface,
				   void *spec)
{
	int ret;
	struct comp_dev *dev;
	struct processing_module *mod;
	struct ipc_config_process *ipc_codec_adapter = spec;

	comp_cl_dbg(drv, "codec_adapter_new() start");

	if (!config) {
		comp_cl_err(drv, "codec_adapter_new(), wrong input params! drv = %x config = %x",
			    (uint32_t)drv, (uint32_t)config);
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev) {
		comp_cl_err(drv, "codec_adapter_new(), failed to allocate memory for comp_dev");
		return NULL;
	}
	dev->ipc_config = *config;
	dev->drv = drv;

	mod = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*mod));
	if (!mod) {
		comp_err(dev, "codec_adapter_new(), failed to allocate memory for module");
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, mod);

	/* Copy setup config */
	ret = load_setup_config(dev, ipc_codec_adapter->data, ipc_codec_adapter->size);
	if (ret) {
		comp_err(dev, "codec_adapter_new() error %d: config loading has failed.",
			 ret);
		goto err;
	}
	/* Init processing codec */
	ret = codec_init(dev, interface);
	if (ret) {
		comp_err(dev, "codec_adapter_new() %d: codec initialization failed",
			 ret);
		goto err;
	}

	dev->state = COMP_STATE_READY;

	comp_dbg(dev, "codec_adapter_new() done");
	return dev;
err:
	rfree(mod);
	rfree(dev);
	return NULL;
}

int validate_setup_config(struct ca_config *cfg)
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
 * The latter is send in a TLV format organized by struct module_param.
 *
 * \return integer representing either:
 *	0 -> success
 *	negative value -> failure.
 */
int load_setup_config(struct comp_dev *dev, void *cfg, uint32_t size)
{
	int ret;
	void *lib_cfg;
	size_t lib_cfg_size;
	struct processing_module *mod = comp_get_drvdata(dev);

	comp_dbg(dev, "load_setup_config() start.");

	if (!cfg || !size) {
		comp_err(dev, "load_setup_config(): no config available cfg: %x, size: %d",
			 (uintptr_t)cfg, size);
		ret = -EINVAL;
		goto end;
	} else if (size < sizeof(struct ca_config)) {
		comp_err(dev, "load_setup_config(): no codec config available, size %d", size);
		ret = -EIO;
		goto end;
	}
	/* Copy codec_adapter part */
	ret = memcpy_s(&mod->ca_config, sizeof(mod->ca_config), cfg,
		       sizeof(struct ca_config));
	assert(!ret);
	ret = validate_setup_config(&mod->ca_config);
	if (ret) {
		comp_err(dev, "load_setup_config(): validation of setup config for codec_adapter failed.");
		goto end;
	}
	/* Copy codec specific part */
	lib_cfg_size = size - sizeof(struct ca_config);
	if (lib_cfg_size) {
		lib_cfg = (char *)cfg + sizeof(struct ca_config);
		ret = codec_load_config(dev, lib_cfg, lib_cfg_size, MODULE_CFG_SETUP);
		if (ret) {
			comp_err(dev, "load_setup_config(): %d: failed to load setup config for codec id %x",
				 ret, mod->ca_config.codec_id);
			goto end;
		}
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
int codec_adapter_prepare(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	uint32_t buff_periods = 2; /* default periods of local buffer,
				    * may change if case of deep buffering
				    */
	uint32_t buff_size; /* size of local buffer */

	comp_dbg(dev, "codec_adapter_prepare() start");

	/* Init sink & source buffers */
	mod->ca_sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	mod->ca_source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);

	if (!mod->ca_source) {
		comp_err(dev, "codec_adapter_prepare(): source buffer not found");
		return -EINVAL;
	} else if (!mod->ca_sink) {
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

	/* Codec is prepared, now we need to configure processing settings.
	 * If codec internal buffer is not equal to natural multiple of pipeline
	 * buffer we have a situation where CA have to deep buffer certain amount
	 * of samples on its start (typically few periods) in order to regularly
	 * generate output once started (same situation happens for compress streams
	 * as well).
	 */
	if (md->mpd.in_buff_size != mod->period_bytes) {
		if (md->mpd.in_buff_size > mod->period_bytes) {
			buff_periods = (md->mpd.in_buff_size % mod->period_bytes) ?
				       (md->mpd.in_buff_size / mod->period_bytes) + 2 :
				       (md->mpd.in_buff_size / mod->period_bytes) + 1;
		} else {
			buff_periods = (mod->period_bytes % md->mpd.in_buff_size) ?
				       (mod->period_bytes / md->mpd.in_buff_size) + 2 :
				       (mod->period_bytes / md->mpd.in_buff_size) + 1;
		}

		mod->deep_buff_bytes = mod->period_bytes * buff_periods;
	} else {
		mod->deep_buff_bytes = 0;
	}

	/* Allocate local buffer */
	buff_size = MAX(mod->period_bytes, md->mpd.out_buff_size) * buff_periods;
	if (mod->local_buff) {
		ret = buffer_set_size(mod->local_buff, buff_size);
		if (ret < 0) {
			comp_err(dev, "codec_adapter_prepare(): buffer_set_size() failed, buff_size = %u",
				 buff_size);
			return ret;
		}
	} else {
		mod->local_buff = buffer_alloc(buff_size, SOF_MEM_CAPS_RAM, PLATFORM_DCACHE_ALIGN);
		if (!mod->local_buff) {
			comp_err(dev, "codec_adapter_prepare(): failed to allocate local buffer");
			return -ENOMEM;
		}

	}
	buffer_set_params(mod->local_buff, &mod->stream_params,
			  BUFFER_UPDATE_FORCE);
	buffer_reset_pos(mod->local_buff, NULL);

	comp_dbg(dev, "codec_adapter_prepare() done");

	return 0;
}

int codec_adapter_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "codec_adapter_params(): comp_verify_params() failed.");
		return ret;
	}

	ret = memcpy_s(&mod->stream_params, sizeof(struct sof_ipc_stream_params),
		       params, sizeof(struct sof_ipc_stream_params));
	assert(!ret);

	mod->period_bytes = params->sample_container_bytes *
			   params->channels * params->rate / 1000;
	return 0;
}

static void
codec_adapter_copy_from_source_to_lib(const struct audio_stream *source,
				      const struct module_processing_data *mpd,
				      size_t bytes)
{
	/* head_size - available data until end of local buffer */
	const int without_wrap = audio_stream_bytes_without_wrap(source, source->r_ptr);
	uint32_t head_size = MIN(bytes, without_wrap);
	/* tail_size - residue data to be copied starting from the beginning
	 * of the buffer
	 */
	uint32_t tail_size = bytes - head_size;
	/* copy head_size to lib buffer */
	memcpy_s(mpd->in_buff, mpd->in_buff_size, source->r_ptr, head_size);
	if (tail_size)
	/* copy reset of the samples after wrap-up */
		memcpy_s((char *)mpd->in_buff + head_size, mpd->in_buff_size,
			 audio_stream_wrap(source,
					   (char *)source->r_ptr + head_size),
					   tail_size);
}

static void
codec_adapter_copy_from_lib_to_sink(const struct module_processing_data *mpd,
				    const struct audio_stream *sink,
				    size_t bytes)
{
	/* head_size - free space until end of local buffer */
	const int without_wrap =
		audio_stream_bytes_without_wrap(sink, sink->w_ptr);
	uint32_t head_size = MIN(bytes, without_wrap);
	/* tail_size - rest of the bytes that needs to be written
	 * starting from the beginning of the buffer
	 */
	uint32_t tail_size = bytes - head_size;

	/* copy head_size to sink buffer */
	memcpy_s(sink->w_ptr, sink->size, mpd->out_buff, head_size);
	if (tail_size)
	/* copy reset of the samples after wrap-up */
		memcpy_s(audio_stream_wrap(sink,
					   (char *)sink->w_ptr + head_size),
			 sink->size,
			 (char *)mpd->out_buff + head_size, tail_size);
}

/**
 * \brief Generate zero samples of "bytes" size for the sink.
 * \param[in] sink - a pointer to sink buffer.
 * \param[in] bytes - number of zero bytes to produce.
 *
 * \return: none.
 */
static void generate_zeroes(struct comp_buffer *sink, uint32_t bytes)
{
	uint32_t tmp, copy_bytes = bytes;
	void *ptr;

	while (copy_bytes) {
		ptr = audio_stream_wrap(&sink->stream, sink->stream.w_ptr);
		tmp = audio_stream_bytes_without_wrap(&sink->stream,
						      ptr);
		tmp = MIN(tmp, copy_bytes);
		ptr = (char *)ptr + tmp;
		copy_bytes -= tmp;
	}
	comp_update_buffer_produce(sink, bytes);
}

int codec_adapter_copy(struct comp_dev *dev)
{
	int ret = 0;
	uint32_t bytes_to_process, copy_bytes, processed = 0, produced = 0;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	struct comp_buffer *source = mod->ca_source;
	struct comp_buffer *sink = mod->ca_sink;
	uint32_t codec_buff_size = md->mpd.in_buff_size;
	struct comp_buffer *local_buff = mod->local_buff;
	struct comp_copy_limits cl;

	comp_get_copy_limits_with_lock(source, local_buff, &cl);
	bytes_to_process = cl.frames * cl.source_frame_bytes;

	comp_dbg(dev, "codec_adapter_copy() start: codec_buff_size: %d, local_buff free: %d source avail %d",
		 codec_buff_size, local_buff->stream.free, source->stream.avail);

	/* Proceed only if we have enough data to fill the lib buffer
	 * completely. If you don't fill whole buffer
	 * the lib won't process it.
	 */
	if (bytes_to_process < codec_buff_size) {
		comp_dbg(dev, "codec_adapter_copy(): source has less data than codec buffer size - processing terminated.");
		goto db_verify;
	}

	if (!md->mpd.init_done) {
		buffer_stream_invalidate(source, codec_buff_size);
		codec_adapter_copy_from_source_to_lib(&source->stream, &md->mpd,
						      codec_buff_size);
		md->mpd.avail = codec_buff_size;
		ret = codec_process(dev);
		if (ret)
			return ret;

		bytes_to_process -= md->mpd.consumed;
		processed += md->mpd.consumed;
		comp_update_buffer_consume(source, md->mpd.consumed);
		if (bytes_to_process < codec_buff_size)
			goto db_verify;
	}

	buffer_stream_invalidate(source, codec_buff_size);
	codec_adapter_copy_from_source_to_lib(&source->stream, &md->mpd,
					      codec_buff_size);
	md->mpd.avail = codec_buff_size;
	ret = codec_process(dev);
	if (ret) {
		if (ret == -ENOSPC) {
			ret = 0;
			goto db_verify;
		}

		comp_err(dev, "codec_adapter_copy() error %x: lib processing failed",
			 ret);
		goto db_verify;
	} else if (md->mpd.produced == 0) {
		/* skipping as lib has not produced anything */
		comp_err(dev, "codec_adapter_copy() error %x: lib hasn't processed anything",
			 ret);
		goto db_verify;
	}
	codec_adapter_copy_from_lib_to_sink(&md->mpd, &local_buff->stream,
					    md->mpd.produced);

	bytes_to_process -= md->mpd.consumed;
	processed += md->mpd.consumed;
	produced += md->mpd.produced;

	audio_stream_produce(&local_buff->stream, md->mpd.produced);
	comp_update_buffer_consume(source, md->mpd.consumed);

db_verify:
	if (!produced && !mod->deep_buff_bytes) {
		comp_dbg(dev, "codec_adapter_copy(): nothing processed in this call");
		/* we haven't produced anything in this period but we
		 * still have data in the local buffer to copy to sink
		 */
		if (audio_stream_get_avail_bytes(&local_buff->stream) >= mod->period_bytes)
			goto copy_period;
		else
			goto end;
	}

	if (mod->deep_buff_bytes) {
		if (mod->deep_buff_bytes >= audio_stream_get_avail_bytes(&local_buff->stream)) {
			generate_zeroes(sink, mod->period_bytes);
			goto end;
		} else {
			comp_dbg(dev, "codec_adapter_copy(): deep buffering has ended after gathering %d bytes of processed data",
				 audio_stream_get_avail_bytes(&local_buff->stream));
			mod->deep_buff_bytes = 0;
		}
	}

copy_period:
	comp_get_copy_limits_with_lock(local_buff, sink, &cl);
	copy_bytes = cl.frames * cl.source_frame_bytes;
	audio_stream_copy(&local_buff->stream, 0,
			  &sink->stream, 0,
			  copy_bytes / mod->stream_params.sample_container_bytes);
	buffer_stream_writeback(sink, copy_bytes);

	comp_update_buffer_produce(sink, copy_bytes);
	comp_update_buffer_consume(local_buff, copy_bytes);
end:
	comp_dbg(dev, "codec_adapter_copy(): processed %d in this call %d bytes left for next period",
		 processed, bytes_to_process);
	return ret;
}

static int codec_adapter_set_params(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
				    enum module_cfg_type type)
{
	int ret;
	char *dst, *src;
	static uint32_t size;
	uint32_t offset;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;

	comp_dbg(dev, "codec_adapter_set_params(): start: num_of_elem %d, elem remain %d msg_index %u",
		 cdata->num_elems, cdata->elems_remaining, cdata->msg_index);

	/* Stage 1 verify input params & allocate memory for the config blob */
	if (cdata->msg_index == 0) {
		size = cdata->num_elems + cdata->elems_remaining;
		/* Check that there is no work-in-progress on previous request */
		if (md->runtime_params) {
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
		} else if (type != MODULE_CFG_SETUP && type != MODULE_CFG_RUNTIME) {
			comp_err(dev, "codec_adapter_set_params() error: unknown config type");
			ret = -EINVAL;
			goto end;
		}

		/* Allocate buffer for new params */
		md->runtime_params = rballoc(0, SOF_MEM_CAPS_RAM, size);
		if (!md->runtime_params) {
			comp_err(dev, "codec_adapter_set_params(): space allocation for new params failed");
			ret = -ENOMEM;
			goto end;
		}

		memset(md->runtime_params, 0, size);
	} else if (!md->runtime_params) {
		comp_err(dev, "codec_adapter_set_params() error: no memory available for runtime params in consecutive load");
		ret = -EIO;
		goto end;
	}

	offset = size - (cdata->num_elems + cdata->elems_remaining);
	dst = (char *)md->runtime_params + offset;
	src = (char *)cdata->data->data;

	ret = memcpy_s(dst, size - offset, src, cdata->num_elems);
	assert(!ret);

	/* Config has been copied now we can load & apply it depending on
	 * codec state.
	 */
	if (!cdata->elems_remaining) {
		switch (type) {
		case MODULE_CFG_SETUP:
			ret = load_setup_config(dev, md->runtime_params, size);
			if (ret) {
				comp_err(dev, "codec_adapter_set_params(): error %d: load of setup config failed.",
					 ret);
			} else {
				comp_dbg(dev, "codec_adapter_set_params() load of setup config done.");
			}

			break;
		case MODULE_CFG_RUNTIME:
			ret = codec_load_config(dev, md->runtime_params, size,
						MODULE_CFG_RUNTIME);
			if (ret) {
				comp_err(dev, "codec_adapter_set_params() error %d: load of runtime config failed.",
					 ret);
				goto done;
			} else {
				comp_dbg(dev, "codec_adapter_set_params() load of runtime config done.");
			}

			if (md->state >= MODULE_INITIALIZED) {
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
				mod->priv.r_cfg.avail = true;
			}

			break;
		default:
			comp_err(dev, "codec_adapter_set_params(): error: unknown config type.");
			break;
		}
	} else
		goto end;

done:
	if (md->runtime_params)
		rfree(md->runtime_params);
	md->runtime_params = NULL;
	return ret;
end:
	return ret;
}

static int ca_set_binary_data(struct comp_dev *dev,
			      struct sof_ipc_ctrl_data *cdata)
{
	int ret;

	comp_dbg(dev, "ca_set_binary_data() start, data type %d", cdata->data->type);

	switch (cdata->data->type) {
	case MODULE_CFG_SETUP:
	case MODULE_CFG_RUNTIME:
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
	struct processing_module *mod = comp_get_drvdata(dev);

	comp_dbg(dev, "codec_adapter_ctrl_set_data() start, state %d, cmd %d",
		 mod->priv.state, cdata->cmd);

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
int codec_adapter_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	int ret;
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);

	comp_dbg(dev, "codec_adapter_cmd() %d start", cmd);

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

	comp_dbg(dev, "codec_adapter_cmd() done");
	return ret;
}

int codec_adapter_trigger(struct comp_dev *dev, int cmd)
{
	comp_dbg(dev, "codec_adapter_trigger(): component got trigger cmd %x", cmd);

	return comp_set_state(dev, cmd);
}

int codec_adapter_reset(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);

	comp_dbg(dev, "codec_adapter_reset(): resetting");

	ret = codec_reset(dev);
	if (ret) {
		comp_err(dev, "codec_adapter_reset(): error %d, codec reset has failed",
			 ret);
	}
	buffer_zero(mod->local_buff);

	comp_dbg(dev, "codec_adapter_reset(): done");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

void codec_adapter_free(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);

	comp_dbg(dev, "codec_adapter_free(): start");

	ret = codec_free(dev);
	if (ret) {
		comp_err(dev, "codec_adapter_free(): error %d, codec reset has failed",
			 ret);
	}
	buffer_free(mod->local_buff);
	rfree(mod);
	rfree(dev);
}

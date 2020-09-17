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
#include <sof/audio/pipeline.h>
#include <sof/audio/codec_adapter/codec_adapter.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <user/trace.h>

static const struct comp_driver comp_codec_adapter;

/* d8218443-5ff3-4a4c-b388-6cfe07b956aa */
DECLARE_SOF_RT_UUID("codec_adapter", ca_uuid, 0xd8218443, 0x5ff3, 0x4a4c,
		    0xb3, 0x88, 0x6c, 0xfe, 0x07, 0xb9, 0x56, 0xaa);

DECLARE_TR_CTX(ca_tr, SOF_UUID(ca_uuid), LOG_LEVEL_INFO);

/**
 * \brief Load setup config for both codec adapter and codec library.
 * \param[in] dev - codec adapter component device pointer.
 * \param[in] cfg - pointer to the configuration data.
 * \param[in] size - size of config.
 *
 * The setup config comprises of two parts - one contains essential data
 * for the initialization of codec_adapter and follows struct ca_config.
 * Second contains codec specific data needed to setup the codec itself.
 * The leter is send in a TLV format organized by struct codec_param.
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
		comp_err(dev, "load_setup_config() error: no component device.");
		ret = -EINVAL;
		goto end;
	} else if (!cfg || !size) {
		comp_err(dev, "load_setup_config() error: no config available cfg: %x, size: %d",
			 (uintptr_t)cfg, size);
		ret = -EINVAL;
		goto end;
	} else if (size <= sizeof(struct ca_config)) {
		comp_err(dev, "load_setup_config() error: no codec config available.");
		ret = -EIO;
		goto end;
	}

	/* Copy codec_adapter part */
	ret = memcpy_s(&cd->ca_config, sizeof(cd->ca_config), cfg,
		       sizeof(struct ca_config));
	assert(!ret);
	ret = validate_setup_config(&cd->ca_config);
	if (ret) {
		comp_err(dev, "load_setup_config(): error: validation of setup config for codec_adapter failed.");
		goto end;
	}
	/* Copy codec specific part */
	lib_cfg = (char *)cfg + sizeof(struct ca_config);
	lib_cfg_size = size - sizeof(struct ca_config);
	ret = codec_load_config(dev, lib_cfg, lib_cfg_size, CODEC_CFG_SETUP);
	if (ret) {
		comp_err(dev, "load_setup_config(): error %d: failed to load setup config for codec id %x",
			 ret, cd->ca_config.codec_id);
		goto end;
	} else {
		comp_dbg(dev, "load_setup_config() codec config loaded successfully.");
	}

	comp_dbg(dev, "load_setup_config() done.");
end:
	return ret;
}

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

	/* Copy setup config for both codec_adapter & codec */
	ret = load_setup_config(dev, ipc_codec_adapter->data, ipc_codec_adapter->size);
	if (ret) {
		comp_err(dev, "codec_adapter_new() error %d: config loading has failed.",
			 ret);
		goto err;
	}

	/* Init processing codec */
	ret = codec_init(dev);
	if (ret) {
		comp_err(dev, "codec_adapter_new() error %d: codec initialization failed",
			 ret);
		goto err;
	}

	dev->state = COMP_STATE_READY;
	cd->state = PP_STATE_CREATED;
	cd->runtime_params = NULL;

	comp_cl_info(&comp_codec_adapter, "codec_adapter_new() done.");
	return dev;
err:
	rfree(cd);
	rfree(dev);
	return NULL;
}

static int codec_adapter_verify_params(struct comp_dev *dev,
			     	struct sof_ipc_stream_params *params)
{
	/* TODO check on GP level. Params to codec shall me sent
	 * in dedicated binary_set ipc
	 */
	return 0;
}

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
                comp_err(dev, "codec_adapter_prepare() erro: source buffer not found");
                return -EINVAL;
        } else if (!cd->ca_sink) {
                comp_err(dev, "codec_adapter_prepare() erro: sink buffer not found");
                return -EINVAL;
        }

	/* Are we already prepared? */
	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET) {
		comp_err(dev, "codec_adapter_prepare() error %x: codec_adapter has already been prepared",
			 ret);
		return PPL_STATUS_PATH_STOP;
	}

	/* Prepare codec */
	ret = codec_prepare(dev);
	if (ret) {
		comp_err(dev, "codec_adapter_prepare() error %x: codec prepare failed",
			 ret);

		return -EIO;
	} else {
		comp_info(dev, "codec_adapter_prepare() codec prepared successfully");
	}

	comp_info(dev, "codec_adapter_prepare() done");
        cd->state = PP_STATE_PREPARED;

	return 0;
}

static int codec_adapter_params(struct comp_dev *dev,
				    struct sof_ipc_stream_params *params)
{
	int ret = 0;

	if (dev->state == COMP_STATE_PREPARE) {
		comp_warn(dev, "codec_adapter_params(): params has already been prepared.");
		goto end;
	}

	ret = codec_adapter_verify_params(dev, params);
	if (ret < 0) {
		comp_err(dev, "codec_adapter_params(): pcm params verification failed");
		goto end;
	}

end:
	return ret;
}

static void codec_adapter_copy_to_lib(const struct audio_stream *source,
			      void *lib_buff, size_t size)
{
	void *src;
	void *dst = lib_buff;
	size_t i;
	size_t j = 0;
	size_t channel;
	size_t channels = source->channels;
	size_t sample_width = source->frame_fmt == SOF_IPC_FRAME_S16_LE ?
			  16 : 32;
	size_t frames = size / (sample_width / 8 * channels);

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < channels; channel++) {
			switch (sample_width) {
			case 16:
				src = audio_stream_read_frag_s16(source, j);
				*((int16_t *)dst) = *((int16_t *)src);
				dst = ((int16_t *)dst) + 1;
				break;
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
			case 24:
			case 32:
				src = audio_stream_read_frag_s32(source, j);
				*((int32_t *)dst) = *((int32_t *)src);
				dst = ((int32_t *)dst) + 1;
				break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE*/
			default:
				comp_cl_info(&comp_codec_adapter, "codec_adapter_copy_to_lib(): An attempt to copy not supported format!");
				return;
			}
			j++;
		}
	}

}

static void codec_adapter_copy_from_lib_to_sink(void *source, struct audio_stream *sink,
			      size_t size)
{
	void *dst;
	void *src = source;
	size_t i;
	size_t j = 0;
	size_t channel;
	size_t sample_width = sink->frame_fmt == SOF_IPC_FRAME_S16_LE ?
			  16 : 32;
	size_t channels = sink->channels;
	size_t frames = size / (sample_width / 8 * channels);


	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < channels; channel++) {
			switch (sample_width) {
#if CONFIG_FORMAT_S16LE
			case 16:
				dst = audio_stream_write_frag_s16(sink, j);
				*((int16_t *)dst) = *((int16_t *)src);
				src = ((int16_t *)src) + 1;
				break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
			case 24:
			case 32:
				dst = audio_stream_write_frag_s32(sink, j);
				*((int32_t *)dst) = *((int32_t *)src);
				src = ((int32_t *)src) + 1;
				break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */
			default:
				comp_cl_info(&comp_codec_adapter, "codec_adapter_copy_to_lib(): An attempt to copy not supported format!");
				return;
			}
			j++;
		}
	}

}

static int codec_adapter_copy(struct comp_dev *dev)
{
	int ret = 0;
	uint32_t copy_bytes, bytes_to_process, processed = 0;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_data *codec = &cd->codec;
	struct comp_buffer *source = cd->ca_source;
	struct comp_buffer *sink = cd->ca_sink;
        uint32_t lib_buff_size = codec->cpd.in_buff_size;

	struct comp_copy_limits c;

	comp_get_copy_limits_with_lock(source, sink, &c);
	bytes_to_process = c.frames * audio_stream_frame_bytes(&source->stream);

        bytes_to_process = MIN(sink->stream.free, source->stream.avail);
	copy_bytes = MIN(sink->stream.free, source->stream.avail);

        comp_info(dev, "codec_adapter_copy() start lib_buff_size: %d, sink free: %d source avail %d copy_bytes %d",
        	  lib_buff_size, sink->stream.free, source->stream.avail, copy_bytes);


	buffer_invalidate(source, lib_buff_size);
	while (bytes_to_process) {
		if (bytes_to_process < lib_buff_size) {
			comp_info(dev, "codec_adapter_copy(): processed %d in this call %d bytes left for next period",
			        processed, bytes_to_process);
			break;
		}

		/* Fill lib buffer completely. NOTE! If you don't fill whole buffer
		 * the lib won't process it.
		 */
		codec_adapter_copy_to_lib(&source->stream,
					  codec->cpd.in_buff,
					  lib_buff_size);
		codec->cpd.avail = lib_buff_size;
		ret = codec_process(dev);
		if (ret) {
			comp_err(dev, "codec_adapter_copy() error %x: lib processing failed",
				 ret);
			break;
		} else if (codec->cpd.produced == 0) {
			/* skipping as lib has not produced anything */
                        comp_info(dev, "codec_adapter_copy() error %x: lib hasn't processed anything",
                                 ret);
			break;
		}

                codec_adapter_copy_from_lib_to_sink(codec->cpd.out_buff,
                				    &sink->stream, codec->cpd.produced);
		bytes_to_process -= codec->cpd.produced;
		processed += codec->cpd.produced;
	}

        //kpb_copy_samples(sink, source, copy_bytes);
	if (!processed) {
		comp_info(dev, "codec_adapter_copy() error: failed to process anything in this call!");
		goto end;
	} else {
		comp_info(dev, "codec_adapter_copy: codec processed %d bytes", processed);
	}

	buffer_writeback(sink, processed);
	comp_update_buffer_produce(sink, lib_buff_size);
	comp_update_buffer_consume(source, lib_buff_size);
end:
        comp_info(dev, "codec_adapter_copy() end processed: %d", processed);
	return ret;
}

static void codec_adapter_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_cl_info(&comp_codec_adapter, "codec_adapter_free(): start");

	rfree(cd);
	rfree(dev);
	//TODO: call lib API to free its resources

	comp_cl_info(&comp_codec_adapter, "codec_adapter_free(): component memory freed");

}

static int codec_adapter_trigger(struct comp_dev *dev, int cmd)
{
	comp_cl_info(&comp_codec_adapter, "codec_adapter_trigger(): component got trigger cmd %x",
		     cmd);

	//TODO: ask lib if pp parameters has been aplied and if not log it!
        //likely here change detect COMP_TRIGGER_START cmd and change state to PP_STATE_RUN
	return comp_set_state(dev, cmd);
}

static int codec_adapter_reset(struct comp_dev *dev)
{
        struct comp_data *cd = comp_get_drvdata(dev);

	comp_cl_info(&comp_codec_adapter, "codec_adapter_reset(): resetting");

        cd->state = PP_STATE_CREATED;
        //TODO: reset codec params

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int ca_set_params(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
			 enum codec_cfg_type type)
{
	int ret;
	char *dst, *src;
	static uint32_t size;
	uint32_t offset;
        struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "ca_set_runtime_params(): start: num_of_elem %d, elem remain %d msg_index %u",
		  cdata->num_elems, cdata->elems_remaining, cdata->msg_index);

	/* Stage 1 load whole config locally */
	/* Check that there is no work-in-progress previous request */
	if (cd->runtime_params && cdata->msg_index == 0) {
		comp_err(dev, "ca_set_runtime_params() error: busy with previous request");
		ret = -EINVAL;
		goto end;
	}


	if (cdata->num_elems + cdata->elems_remaining > MAX_BLOB_SIZE)
	{
		comp_err(dev, "ca_set_runtime_params() error: blob size is too big!");
		ret = -EINVAL;
		goto end;
	}

	if (cdata->msg_index == 0) {
		/* Allocate buffer for new params */
		size = cdata->num_elems + cdata->elems_remaining;
		//todo: potentuial leakage again here
		cd->runtime_params = rballoc(0, SOF_MEM_CAPS_RAM, size);

		if (!cd->runtime_params) {
			comp_err(dev, "ca_set_runtime_params(): space allocation for new params failed");
			ret = -ENOMEM;
			goto end;
		}
		memset(cd->runtime_params, 0, size);
	}

	offset = size - (cdata->num_elems + cdata->elems_remaining);
	dst = (char *)cd->runtime_params + offset;
	src = (char *)cdata->data->data;

	ret = memcpy_s(dst,
		       size - offset,
		       src, cdata->num_elems);

	assert(!ret);

	if (cdata->elems_remaining == 0 && size) {
		/* Config has been copied now we can load & apply it
		 * depending on lib status.
		 */
		//rework to switch case here!!
		if (type == CODEC_CFG_SETUP) {
			ret = memcpy_s(&cd->ca_config, sizeof(cd->ca_config), cd->runtime_params,
				       sizeof(cd->ca_config));
			assert(!ret);
			ret = validate_setup_config(&cd->ca_config);
			if (ret) {
				comp_err(dev, "XXXX(): error: validation of setup config failed");
				goto end;
			}

			/* Pass config further to the codec */
			void *lib_cfg;
			uint32_t lib_cfg_size;
			lib_cfg = (char *)cd->runtime_params + sizeof(struct ca_config);
			lib_cfg_size = size - sizeof(struct ca_config);
			ret = codec_load_config(dev, lib_cfg, lib_cfg_size,
						CODEC_CFG_SETUP);
			if (ret) {
				comp_err(dev, "XXXX(): error %d: failed to load setup config for codec",
					 ret);
			} else {
				comp_dbg(dev, "XXXX() codec config loaded successfully");
			}
			goto end;
		} else {
			ret = codec_load_config(dev, cd->runtime_params, size,
						type);
			if (ret) {
				comp_err(dev, "ca_set_runtime_params() error %x: lib params load failed",
					 ret);
				goto end;
	        	}

		}

		if (cd->state >= PP_STATE_PREPARED && type == CODEC_CFG_RUNTIME) {
			/* Post processing is already prepared so we can apply runtime
			 * config right away.
			 */
			ret = codec_apply_runtime_config(dev);
			if (ret) {
				comp_err(dev, "codec_adapter_ctrl_set_data() error %x: lib config apply failed",
					 ret);
				goto end;
			}
		} else {
			cd->codec.r_cfg.avail = true;
		}
	}

end:
//THIS need to reworked in 3 labels err & done both freee the memory but normal end does not
// since if w load large blob of data we need this pointer to stay in concecutive calls
	if (cd->runtime_params)
		rfree(cd->runtime_params);
	cd->runtime_params = NULL;
	return ret;
}

static int ca_set_binary_data(struct comp_dev *dev,
			      struct sof_ipc_ctrl_data *cdata) {
	int ret;

	 comp_info(dev, "ca_set_binary_data() start, data type %d",
	 	   cdata->data->type);

	switch (cdata->data->type) {
		/* TODO: use enum hifi_codec_cfg_type here */
	case CODEC_CFG_SETUP:
	case CODEC_CFG_RUNTIME:
		ret = ca_set_params(dev, cdata, cdata->data->type);
		break;
	default:
		comp_err(dev, "ca_set_binary_data() error: unknown binary data type");
		ret = -EIO;
		break;
	}

	return ret;
}

static int codec_adapter_ctrl_set_data(struct comp_dev *dev,
                                      struct sof_ipc_ctrl_data *cdata) {
	int ret;

	struct comp_data *cd = comp_get_drvdata(dev);

        comp_info(dev, "codec_adapter_ctrl_set_data() start, state %d, cmd %d",
        	  cd->state, cdata->cmd);

	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		comp_err(dev, "codec_adapter_ctrl_set_data(): ABI mismatch");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		//TODO
		ret = -EINVAL;
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
	struct sof_ipc_ctrl_data *cdata = data;

	comp_info(dev, "codec_adapter_cmd() %d start", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return codec_adapter_ctrl_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		//TODO
		return -EINVAL;
	default:
		comp_err(dev, "codec_adapter_cmd() error: unknown command");
		return -EINVAL;
	}
}
static const struct comp_driver comp_codec_adapter = {
	.type = SOF_COMP_NONE,
	.uid = SOF_RT_UUID(ca_uuid),
	.tctx = &ca_tr,
	.ops = {
		.create = codec_adapter_new,
		.params = codec_adapter_params,
		.prepare = codec_adapter_prepare,
		.copy = codec_adapter_copy,
		.free = codec_adapter_free,
		.trigger = codec_adapter_trigger,
		.reset = codec_adapter_reset,
		.cmd = codec_adapter_cmd,
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

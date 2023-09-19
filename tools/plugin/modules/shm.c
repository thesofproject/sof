// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.

/* shm component for reading/writing pcm samples to/from a shm */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

#include <rtos/sof.h>
#include <sof/list.h>
#include <sof/audio/stream.h>
#include <sof/audio/ipc-config.h>
#include <sof/ipc/driver.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <ipc/stream.h>
#include <ipc/topology.h>

#include "pipe.h"

/* 1488beda-e847-ed11-b309-a58b974fecce */
DECLARE_SOF_RT_UUID("shmread", shmread_uuid, 0xdabe8814, 0x47e8, 0x11ed,
		    0xa5, 0x8b, 0xb3, 0x09, 0x97, 0x4f, 0xec, 0xce);
DECLARE_TR_CTX(shmread_tr, SOF_UUID(shmread_uuid), LOG_LEVEL_INFO);

/* 1c03b6e2-e847-ed11-7f80-07a91b6efa6c */
DECLARE_SOF_RT_UUID("shmwrite", shmwrite_uuid, 0xe2b6031c, 0x47e8, 0x11ed,
		    0x07, 0xa9, 0x7f, 0x80, 0x1b, 0x6e, 0xfa, 0x6c);
DECLARE_TR_CTX(shmwrite_tr, SOF_UUID(shmwrite_uuid), LOG_LEVEL_INFO);

static const struct comp_driver comp_shmread;
static const struct comp_driver comp_shmwrite;

/* shm comp data */
struct shm_comp_data {
	/* PCM data */
	struct plug_shm_desc pcm;
	struct plug_shm_endpoint *ctx;
#if CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
#endif
};

static int shm_process_new(struct comp_dev *dev,
			   const struct comp_ipc_config *config,
			   const void *spec)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);
	struct plug_shm_endpoint *ctx;
	int ret;

	comp_dbg(dev, "shm new()");

	/* FIXME: use PCM ID to create shm */
	ret = plug_shm_init(&cd->pcm, _sp->topology_name, "pcm", 1);
	if (ret < 0)
		return ret;

	// TODO: get the shm size for the buffer using a better method
	cd->pcm.size = 128 * 1024;

	/* mmap the SHM PCM */
	ret = plug_shm_create(&cd->pcm);
	if (ret < 0)
		return ret;

	cd->ctx = cd->pcm.addr;
	ctx = cd->ctx;
	ctx->buffer_size = cd->pcm.size;
	memset(ctx, 0, sizeof(*ctx));
	ctx->comp_id = config->id;
	ctx->pipeline_id = config->pipeline_id;
	ctx->state = SOF_PLUGIN_STATE_INIT;
	dev->state = COMP_STATE_READY;

	return 0;
}

static void shm_free(struct comp_dev *dev)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);

	cd->ctx = NULL;

	plug_shm_free(&cd->pcm);
	shm_unlink(cd->pcm.name);

	free(cd);
	free(dev);
}

static struct comp_dev *shm_new(const struct comp_driver *drv,
				const struct comp_ipc_config *config,
				const void *spec, int direction)
{
	struct comp_dev *dev;
#if CONFIG_IPC_MAJOR_4
	const struct ipc4_base_module_cfg *base_cfg = (struct ipc4_base_module_cfg *)spec;
#endif
	struct shm_comp_data *cd;
	int err;

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	/* allocate  memory for shm comp data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto error;

	comp_set_drvdata(dev, cd);
	memcpy(&cd->base_cfg, base_cfg, sizeof(struct ipc4_base_module_cfg));

	dev->direction = direction;
	err = shm_process_new(dev, config, spec);
	if (err < 0) {
		free(cd);
		free(dev);
		return NULL;
	}
	dev->direction_set = true;

	dev->state = COMP_STATE_READY;
	return dev;

error:
	free(dev);
	return NULL;
}

static struct comp_dev *shmwrite_new(const struct comp_driver *drv,
				     const struct comp_ipc_config *config,
				     const void *spec)
{
	return shm_new(drv, config, spec, SOF_IPC_STREAM_PLAYBACK);
}

static struct comp_dev *shmread_new(const struct comp_driver *drv,
				    const struct comp_ipc_config *config,
				    const void *spec)
{
	return shm_new(drv, config, spec, SOF_IPC_STREAM_CAPTURE);
}

/**
 * \brief Sets shm component audio stream parameters.
 * \param[in,out] dev Volume base component device.
 * \param[in] params Audio (PCM) stream parameters (ignored for this component)
 * \return Error code.
 *
 * All done in prepare() since we need to know source and sink component params.
 */
static int shmread_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);
	struct plug_shm_endpoint *ctx = cd->ctx;
	int ret;

#if CONFIG_IPC_MAJOR_4
	ipc4_base_module_cfg_to_stream_params(&cd->base_cfg, params);
#endif

	comp_err(dev, "forme_fmt %d channels %d", params->frame_fmt, params->channels);

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "shm_params(): pcm params verification failed.");
		return ret;
	}
	ctx->state = SOF_PLUGIN_STATE_READY;

	return 0;
}

static int shmwrite_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);
	struct plug_shm_endpoint *ctx = cd->ctx;
	int ret;

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "shm_params(): pcm params verification failed.");
		return ret;
	}
	ctx->state = SOF_PLUGIN_STATE_READY;

	return 0;
}

static int shm_trigger(struct comp_dev *dev, int cmd)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);
	struct plug_shm_endpoint *ctx = cd->ctx;

	comp_dbg(dev, "shm_trigger(%d)", cmd);

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		ctx->state = SOF_PLUGIN_STATE_STREAM_RUNNING;
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_PAUSE:
		ctx->state = SOF_PLUGIN_STATE_READY;
		break;
	case COMP_TRIGGER_RESET:
		ctx->state = SOF_PLUGIN_STATE_INIT;
		break;
	case COMP_TRIGGER_XRUN:
		ctx->state = SOF_PLUGIN_STATE_STREAM_ERROR;
		break;
	default:
		break;
	}

	return comp_set_state(dev, cmd);
}

static int shm_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	return 0;
}

/*
 * copy from local SOF buffer to remote SHM buffer
 */
static int shmread_copy(struct comp_dev *dev)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);
	struct plug_shm_endpoint *ctx = cd->ctx;
	struct comp_buffer *buffer;
	struct audio_stream *source;
	unsigned int copy_bytes;
	unsigned int remaining;
	unsigned int total = 0;
	void *rptr;
	void *dest;

	/* local SOF source buffer */
	buffer = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	source = &buffer->stream;
	rptr = source->r_ptr;

	/* remote SHM sink buffer */
	dest = plug_ep_wptr(ctx);

	/* maximum byte count that can be copied this iteration */
	remaining = MIN(audio_stream_get_avail_bytes(source), plug_ep_get_free(ctx));

	while (remaining) {
		/* min bytes from source pipe */
		copy_bytes = MIN(remaining, plug_ep_wrap_wsize(ctx));

		/* min bytes from source and sink */
		copy_bytes = MIN(copy_bytes, audio_stream_bytes_without_wrap(source, rptr));

		/* anything to copy ? */
		if (copy_bytes == 0)
			break;

		/* copy to local buffer from SHM buffer */
		memcpy(dest, rptr, copy_bytes);

		/* update SHM pointer with wrap */
		dest = plug_ep_produce(ctx, copy_bytes);

		/* update local pointers */
		rptr = audio_stream_wrap(source, rptr + copy_bytes);

		/* update avail and totals */
		remaining -= copy_bytes;
		total += copy_bytes;
	}

	/* update sink buffer pointers */
	comp_update_buffer_consume(buffer, total);
	comp_dbg(dev, "wrote %d bytes", total);

	return 0;
}

/*
 * copy to local SOF buffer from remote SHM buffer
 */
static int shmwrite_copy(struct comp_dev *dev)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);
	struct plug_shm_endpoint *ctx = cd->ctx;
	struct comp_buffer *buffer;
	struct audio_stream *sink;
	unsigned int copy_bytes;
	unsigned int remaining;
	unsigned int total = 0;
	void *wptr;
	void *src;

	/* local SOF sink buffer */
	buffer = list_first_item(&dev->bsink_list, struct comp_buffer,
				 source_list);
	sink = &buffer->stream;
	wptr = sink->w_ptr;

	/* remote SHM source buffer */
	src = plug_ep_rptr(ctx);

	/* maximum byte count that can be copied this iteration */
	remaining = MIN(audio_stream_get_free_bytes(sink), plug_ep_get_avail(ctx));

	while (remaining > 0) {
		/* min bytes free bytes in local sink */
		copy_bytes = MIN(remaining, plug_ep_wrap_rsize(ctx));

		/* min bytes to wrap */
		copy_bytes = MIN(copy_bytes, audio_stream_bytes_without_wrap(sink, wptr));

		/* nothing to copy ? */
		if (copy_bytes == 0)
			break;

		/* copy to SHM from local buffer */
		memcpy(wptr, src, copy_bytes);

		/* update local pointers */
		wptr = audio_stream_wrap(sink, wptr + copy_bytes);

		/* update SHM pointer with wrap */
		src = plug_ep_consume(ctx, copy_bytes);

		/* update avail and totals */
		remaining -= copy_bytes;
		total += copy_bytes;
	}

	/* update sink buffer pointers */
	comp_update_buffer_produce(buffer, total);
	comp_dbg(dev, "read %d bytes", total);

	return 0;
}

static int shm_prepare(struct comp_dev *dev)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);
	struct plug_shm_endpoint *ctx = cd->ctx;
	int ret = 0;

	comp_dbg(dev, "shm prepare_copy()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	ctx->state = SOF_PLUGIN_STATE_READY;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return ret;
}

static int shm_reset(struct comp_dev *dev)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);
	struct plug_shm_endpoint *ctx = cd->ctx;

	comp_set_state(dev, COMP_TRIGGER_RESET);
	ctx->state = SOF_PLUGIN_STATE_INIT;

	return 0;
}

int shm_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct shm_comp_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		memcpy_s(value, sizeof(struct ipc4_base_module_cfg),
			 &cd->base_cfg, sizeof(struct ipc4_base_module_cfg));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct comp_driver comp_shmread = {
	.type = SOF_COMP_HOST,
	.uid = SOF_RT_UUID(shmread_uuid),
	.tctx	= &shmread_tr,
	.ops = {
		.create = shmread_new,
		.free = shm_free,
		.params = shmread_params,
		.cmd = shm_cmd,
		.trigger = shm_trigger,
		.copy = shmread_copy,
		.prepare = shm_prepare,
		.reset = shm_reset,
		.get_attribute = shm_get_attribute,
	},
};

static const struct comp_driver comp_shmwrite = {
	.type = SOF_COMP_HOST,
	.uid = SOF_RT_UUID(shmwrite_uuid),
	.tctx	= &shmwrite_tr,
	.ops = {
		.create = shmwrite_new,
		.free = shm_free,
		.params = shmwrite_params,
		.cmd = shm_cmd,
		.trigger = shm_trigger,
		.copy = shmwrite_copy,
		.prepare = shm_prepare,
		.reset = shm_reset,
		.get_attribute = shm_get_attribute,
	},
};

static struct comp_driver_info comp_shmread_info = {
	.drv = &comp_shmread,
};

static struct comp_driver_info comp_shmwrite_info = {
	.drv = &comp_shmwrite,
};

static void sys_comp_shm_init(void)
{
	comp_register(&comp_shmread_info);
	comp_register(&comp_shmwrite_info);
}

DECLARE_MODULE(sys_comp_shm_init);

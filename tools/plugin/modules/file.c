// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.

/* file component for reading/writing pcm samples to/from a file */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

/* bfc7488c-75aa-4ce8-9bde-d8da08a698c2 */
DECLARE_SOF_RT_UUID("fileread", fileread_uuid, 0xbfc7488c, 0x75aa, 0x4ce8,
		    0x9d, 0xbe, 0xd8, 0xda, 0x08, 0xa6, 0x98, 0xc2);
DECLARE_TR_CTX(fileread_tr, SOF_UUID(fileread_uuid), LOG_LEVEL_INFO);

/* f599ca2c-15ac-11ed-a969-5329b9cdfd2e */
DECLARE_SOF_RT_UUID("filewrite", filewrite_uuid, 0xf599ca2c, 0x15ac, 0x11ed,
		    0xa9, 0x69, 0x53, 0x29, 0xb9, 0xcd, 0xfd, 0x2e);
DECLARE_TR_CTX(filewrite_tr, SOF_UUID(filewrite_uuid), LOG_LEVEL_INFO);

static const struct comp_driver comp_fileread;
static const struct comp_driver comp_filewrite;

/* file comp data */
struct file_comp_data {
	/* file */
	int fd;
	char *filename;
};

static void file_free(struct comp_dev *dev)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);

	close(cd->fd);
	free(cd);
	free(dev);
}

static struct comp_dev *file_new(const struct comp_driver *drv,
				 const struct comp_ipc_config *config,
				 const void *spec)
{
	struct comp_dev *dev;
	const struct ipc_comp_file *ipc_file = spec;
	struct file_comp_data *cd;

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	/* allocate  memory for file comp data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto error;

	comp_set_drvdata(dev, cd);

	dev->direction = ipc_file->direction;

	dev->state = COMP_STATE_READY;
	return dev;

error:
	free(dev);
	return NULL;
}

static struct comp_dev *fileread_new(const struct comp_driver *drv,
				     const struct comp_ipc_config *config,
				     const void *spec)
{
	struct comp_dev *dev;
	struct file_comp_data *cd;

	dev = file_new(drv, config, spec);
	if (!dev)
		return NULL;

	cd = comp_get_drvdata(dev);
	cd->fd = open(cd->filename, O_RDONLY,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (!cd->fd) {
		fprintf(stderr, "error: opening file %s for reading - %s\n",
			cd->filename, strerror(errno));
		file_free(dev);
		return NULL;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

static struct comp_dev *filewrite_new(const struct comp_driver *drv,
				      const struct comp_ipc_config *config,
				      const void *spec)
{
	struct comp_dev *dev;
	struct file_comp_data *cd;

	dev = file_new(drv, config, spec);
	if (!dev)
		return NULL;
	cd = comp_get_drvdata(dev);

	cd->fd = open(cd->filename, O_CREAT | O_WRONLY,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (!cd->fd) {
		fprintf(stderr, "error: opening file %s for writing - %s\n",
			cd->filename, strerror(errno));
		file_free(dev);
		return NULL;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

/**
 * \brief Sets file component audio stream parameters.
 * \param[in,out] dev Volume base component device.
 * \param[in] params Audio (PCM) stream parameters (ignored for this component)
 * \return Error code.
 *
 * All done in prepare() since we need to know source and sink component params.
 */
static int fileread_params(struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	struct comp_buffer *buffer;
//	struct file_comp_data *cd = comp_get_drvdata(dev);
//	struct audio_stream *stream;
//	int periods;
//	int samples;
	int ret;

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "file_params(): pcm params verification failed.");
		return ret;
	}

	/* file component sink/source buffer period count */
	buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
//	periods = dev->ipc_config.periods_sink;


	/* set downstream buffer size */
//	stream = &buffer->stream;
	buffer_reset_pos(buffer, NULL);

	return 0;
}

static int filewrite_params(struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	struct comp_buffer *buffer;
//	struct file_comp_data *cd = comp_get_drvdata(dev);
//	struct audio_stream *stream;
//	int periods;
//	int samples;
	int ret;

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "file_params(): pcm params verification failed.");
		return ret;
	}

	/* file component sink/source buffer period count */
	buffer = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
//	periods = dev->ipc_config.periods_source;

	/* set downstream buffer size */
//	stream = &buffer->stream;
	buffer_reset_pos(buffer, NULL);

	return 0;
}

static int file_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "file_trigger()");
	return comp_set_state(dev, cmd);
}

/* used to pass standard and bespoke commands (with data) to component */
static int file_cmd(struct comp_dev *dev, int cmd, void *data,
		    int max_data_size)
{
	return 0;
}

/*
 * copy and process stream samples
 * returns the number of bytes copied
 */
static int fileread_copy(struct comp_dev *dev)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *buffer;
	struct audio_stream *sink;
	int remaining_bytes;
	int bytes;
	int ret = 0;
	int total = 0;
	void *pos;

	/* file component sink buffer */
	buffer = list_first_item(&dev->bsink_list, struct comp_buffer,
				 source_list);
	sink = &buffer->stream;
	pos = sink->w_ptr;
	remaining_bytes = audio_stream_get_free_bytes(sink);

	while (remaining_bytes) {
		bytes = audio_stream_bytes_without_wrap(sink, pos);

		/* read PCM samples from file */
		ret = read(cd->fd, pos, bytes);
		if (ret < 0) {
			fprintf(stderr, "failed to read: %s: %s\n",
				cd->filename, strerror(errno));
			return -errno;
		}

		/* eof ? */
		if (ret == 0)
			return ret;

		remaining_bytes -= ret;
		pos = audio_stream_wrap(sink, pos + ret);
		total += ret;
	}

	/* update sink buffer pointers */
	comp_update_buffer_produce(buffer, total);

	return ret;
}

/*
 * copy and process stream samples
 * returns the number of bytes copied
 */
static int filewrite_copy(struct comp_dev *dev)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *buffer;
	struct audio_stream *source;
	int avail;
	int bytes;
	int ret = 0;
	int total = 0;
	void *pos;

	/* file component source buffer */
	buffer = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	source = &buffer->stream;
	pos = source->r_ptr;
	avail = audio_stream_get_avail_bytes(source);

	while (avail) {
		bytes = MIN(avail, audio_stream_bytes_without_wrap(source, pos));

		/* read PCM samples from file */
		ret = write(cd->fd, pos, bytes);
		if (ret < 0) {
			fprintf(stderr, "failed to write: %s: %s\n",
				cd->filename, strerror(errno));
			return -errno;
		}

		avail -= ret;
		pos = audio_stream_wrap(source, pos + ret);
		total += ret;
	}

	/* update sink buffer pointers */
	comp_update_buffer_consume(buffer, total);

	return ret;
}

static int file_prepare(struct comp_dev *dev)
{
	int ret = 0;

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	dev->state = COMP_STATE_PREPARE;
	return ret;
}

static int file_reset(struct comp_dev *dev)
{
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_fileread = {
	.type = SOF_COMP_FILEREAD,
	.uid = SOF_RT_UUID(fileread_uuid),
	.tctx	= &fileread_tr,
	.ops = {
		.create = fileread_new,
		.free = file_free,
		.params = fileread_params,
		.cmd = file_cmd,
		.trigger = file_trigger,
		.copy = fileread_copy,
		.prepare = file_prepare,
		.reset = file_reset,
	},
};

static const struct comp_driver comp_filewrite = {
	.type = SOF_COMP_FILEWRITE,
	.uid = SOF_RT_UUID(filewrite_uuid),
	.tctx	= &filewrite_tr,
	.ops = {
		.create = filewrite_new,
		.free = file_free,
		.params = filewrite_params,
		.cmd = file_cmd,
		.trigger = file_trigger,
		.copy = filewrite_copy,
		.prepare = file_prepare,
		.reset = file_reset,
	},
};

static struct comp_driver_info comp_fileread_info = {
	.drv = &comp_fileread,
};

static struct comp_driver_info comp_filewrite_info = {
	.drv = &comp_filewrite,
};

static void sys_comp_file_init(void)
{
	comp_register(&comp_fileread_info);
	comp_register(&comp_filewrite_info);
}

DECLARE_MODULE(sys_comp_file_init);

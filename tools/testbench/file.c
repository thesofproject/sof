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
#include <sof/sof.h>
#include <sof/list.h>
#include <sof/audio/stream.h>
#include <sof/lib/clk.h>
#include <sof/ipc/driver.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <ipc/stream.h>
#include "testbench/common_test.h"
#include "testbench/file.h"

/* bfc7488c-75aa-4ce8-9bde-d8da08a698c2 */
DECLARE_SOF_UUID("file", file_uuid, 0xbfc7488c, 0x75aa, 0x4ce8,
		 0x9d, 0xbe, 0xd8, 0xda, 0x08, 0xa6, 0x98, 0xc2);
DECLARE_TR_CTX(file_tr, SOF_UUID(file_uuid), LOG_LEVEL_INFO);

static const struct comp_driver comp_file_dai;
static const struct comp_driver comp_file_host;

static inline void buffer_check_wrap_32(int32_t **ptr, int32_t *end,
					size_t size)
{
	if (*ptr >= end)
		*ptr = (int32_t *)((size_t)*ptr - size);
}

static inline void buffer_check_wrap_16(int16_t **ptr, int16_t *end,
					size_t size)
{
	if (*ptr >= end)
		*ptr = (int16_t *)((size_t)*ptr - size);
}

/*
 * Read 32-bit samples from file
 * currently only supports txt files
 */
static int read_samples_32(struct comp_dev *dev,
			   const struct audio_stream *sink,
			   int n, int fmt, int nch)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t sample;
	int n_wrap;
	int n_min;
	int i;
	int n_samples = 0;
	int ret = 0;

	while (n > 0) {
		n_wrap = (int32_t *)sink->end_addr - dest;

		/* check for buffer wrap and copy to the end of the buffer */
		n_min = (n < n_wrap) ? n : n_wrap;
		while (n_min > 0) {
			n -= nch;
			n_min -= nch;

			/* copy sample per channel */
			for (i = 0; i < nch; i++) {
				/* read sample from file */
				switch (cd->fs.f_format) {
				/* text input file */
				case FILE_TEXT:
					if (fmt == SOF_IPC_FRAME_S32_LE)
						ret = fscanf(cd->fs.rfh, "%d",
							     dest);

					/* mask bits if 24-bit samples */
					if (fmt == SOF_IPC_FRAME_S24_4LE) {
						ret = fscanf(cd->fs.rfh, "%d",
							     &sample);
						*dest = sample & 0x00ffffff;
					}
					/* quit if eof is reached */
					if (ret == EOF) {
						cd->fs.reached_eof = 1;
						goto quit;
					}
					break;

				/* raw input file */
				default:
					if (fmt == SOF_IPC_FRAME_S32_LE)
						ret = fread(dest,
							    sizeof(int32_t),
							    1, cd->fs.rfh);

					/* mask bits if 24-bit samples */
					if (fmt == SOF_IPC_FRAME_S24_4LE) {
						ret = fread(&sample,
							    sizeof(int32_t),
							    1, cd->fs.rfh);
						*dest = sample & 0x00ffffff;
					}
					/* quit if eof is reached */
					if (ret != 1) {
						cd->fs.reached_eof = 1;
						goto quit;
					}
					break;
				}
				dest++;
				n_samples++;
			}
		}
		/* check for buffer wrap and update pointer */
		buffer_check_wrap_32(&dest, sink->end_addr,
				     sink->size);
	}
quit:
	return n_samples;
}

/*
 * Read 16-bit samples from file
 * currently only supports txt files
 */
static int read_samples_16(struct comp_dev *dev,
			   const struct audio_stream *sink,
			   int n, int nch)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int16_t *dest = (int16_t *)sink->w_ptr;
	int i, n_wrap, n_min, ret;
	int n_samples = 0;

	/* copy samples */
	while (n > 0) {
		n_wrap = (int16_t *)sink->end_addr - dest;

		/* check for buffer wrap and copy to the end of the buffer */
		n_min = (n < n_wrap) ? n : n_wrap;
		while (n_min > 0) {
			n -= nch;
			n_min -= nch;

			/* copy sample per channel */
			for (i = 0; i < nch; i++) {
				switch (cd->fs.f_format) {
				/* text input file */
				case FILE_TEXT:
					ret = fscanf(cd->fs.rfh, "%hd", dest);
					if (ret == EOF) {
						cd->fs.reached_eof = 1;
						goto quit;
					}
					break;

				/* rw pcm input file */
				default:
					ret = fread(dest, sizeof(int16_t), 1,
						    cd->fs.rfh);
					if (ret != 1) {
						cd->fs.reached_eof = 1;
						goto quit;
					}
					break;
				}

				dest++;
				n_samples++;
			}
		}
		/* check for buffer wrap and update pointer */
		buffer_check_wrap_16(&dest, sink->end_addr,
				     sink->size);
	}

quit:
	return n_samples;
}

/*
 * Write 16-bit samples from file
 * currently only supports txt files
 */
static int write_samples_16(struct comp_dev *dev, struct audio_stream *source,
			    int n, int nch)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int i, n_wrap, n_min, ret;
	int n_samples = 0;

	/* copy samples */
	while (n > 0) {
		n_wrap = (int16_t *)source->end_addr - src;

		/* check for buffer wrap and copy to the end of the buffer */
		n_min = (n < n_wrap) ? n : n_wrap;
		while (n_min > 0) {
			n -= nch;
			n_min -= nch;

			/* copy sample per channel */
			for (i = 0; i < nch; i++) {
				switch (cd->fs.f_format) {
				/* text output file */
				case FILE_TEXT:
					ret = fprintf(cd->fs.wfh,
						      "%d\n", *src);
					if (ret < 0)
						goto quit;
					break;

				/* raw pcm output file */
				default:
					ret = fwrite(src,
						     sizeof(int16_t),
						     1, cd->fs.wfh);
					if (ret != 1)
						goto quit;
					break;
				}

				src++;
				n_samples++;
			}
		}
		/* check for buffer wrap and update pointer */
		buffer_check_wrap_16(&src, source->end_addr,
				     source->size);
	}
quit:
	return n_samples;
}

/*
 * Write 32-bit samples from file
 * currently only supports txt files
 */
static int write_samples_32(struct comp_dev *dev, struct audio_stream *source,
			    int n, int fmt, int nch)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int i, n_wrap, n_min, ret;
	int n_samples = 0;
	int32_t sample;

	/* copy samples */
	while (n > 0) {
		n_wrap = (int32_t *)source->end_addr - src;

		/* check for buffer wrap and copy to the end of the buffer */
		n_min = (n < n_wrap) ? n : n_wrap;
		while (n_min > 0) {
			n -= nch;
			n_min -= nch;

			/* copy sample per channel */
			for (i = 0; i < nch; i++) {
				switch (cd->fs.f_format) {
				/* text output file */
				case FILE_TEXT:
					if (fmt == SOF_IPC_FRAME_S32_LE)
						ret = fprintf(cd->fs.wfh,
							      "%d\n", *src);
					if (fmt == SOF_IPC_FRAME_S24_4LE) {
						sample = *src << 8;
						ret = fprintf(cd->fs.wfh,
							      "%d\n",
							      sample >> 8);
					}
					if (ret < 0)
						goto quit;
					break;

				/* raw pcm output file */
				default:
					if (fmt == SOF_IPC_FRAME_S32_LE)
						ret = fwrite(src,
							     sizeof(int32_t),
							     1, cd->fs.wfh);
					if (fmt == SOF_IPC_FRAME_S24_4LE) {
						sample = *src << 8;
						sample >>= 8;
						ret = fwrite(&sample,
							     sizeof(int32_t),
							     1, cd->fs.wfh);
					}
					if (ret != 1)
						goto quit;
					break;
				}

				/* increment read pointer */
				src++;

				/* increment number of samples written */
				n_samples++;
			}
		}
		/* check for buffer wrap and update pointer */
		buffer_check_wrap_32(&src, source->end_addr,
				     source->size);
	}
quit:
	return n_samples;
}

/* function for processing 32-bit samples */
static int file_s32_default(struct comp_dev *dev, struct audio_stream *sink,
			    struct audio_stream *source, uint32_t frames)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int nch;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = sink->channels;
		n_samples = read_samples_32(dev, sink, frames * nch,
					    SOF_IPC_FRAME_S32_LE, nch);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = source->channels;
		n_samples = write_samples_32(dev, source, frames * nch,
					     SOF_IPC_FRAME_S32_LE, nch);
		break;
	default:
		/* TODO: duplex mode */
		break;
	}

	cd->fs.n += n_samples;
	return n_samples;
}

/* function for processing 16-bit samples */
static int file_s16(struct comp_dev *dev, struct audio_stream *sink,
		    struct audio_stream *source, uint32_t frames)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int nch;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = sink->channels;
		n_samples = read_samples_16(dev, sink, frames * nch, nch);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = source->channels;
		n_samples = write_samples_16(dev, source, frames * nch, nch);
		break;
	default:
		/* TODO: duplex mode */
		break;
	}

	cd->fs.n += n_samples;
	return n_samples;
}

/* function for processing 24-bit samples */
static int file_s24(struct comp_dev *dev, struct audio_stream *sink,
		    struct audio_stream *source, uint32_t frames)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int nch;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = sink->channels;
		n_samples = read_samples_32(dev, sink, frames * nch,
					    SOF_IPC_FRAME_S24_4LE, nch);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = source->channels;
		n_samples = write_samples_32(dev, source, frames * nch,
					     SOF_IPC_FRAME_S24_4LE, nch);
		break;
	default:
		/* TODO: duplex mode */
		break;
	}

	cd->fs.n += n_samples;
	return n_samples;
}

static enum file_format get_file_format(char *filename)
{
	char *ext = strrchr(filename, '.');

	if (!strcmp(ext, ".txt"))
		return FILE_TEXT;

	return FILE_RAW;
}

static struct comp_dev *file_new(const struct comp_driver *drv,
				 struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_file *file;
	struct sof_ipc_comp_file *ipc_file =
		(struct sof_ipc_comp_file *)comp;
	struct file_comp_data *cd;

	debug_print("file_new()\n");

	if (IPC_IS_SIZE_INVALID(ipc_file->config)) {
		fprintf(stderr, "error: file_new() Invalid IPC size.\n");
		return NULL;
	}

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_file));
	if (!dev)
		return NULL;

	file = COMP_GET_IPC(dev, sof_ipc_comp_file);
	assert(!memcpy_s(file, sizeof(*file), ipc_file,
		       sizeof(struct sof_ipc_comp_file)));

	/* allocate  memory for file comp data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		free(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	/* default function for processing samples */
	cd->file_func = file_s32_default;

	/* get filename from IPC and open file */
	cd->fs.fn = strdup(ipc_file->fn);

	/* set file format */
	cd->fs.f_format = get_file_format(cd->fs.fn);

	/* set file comp mode */
	cd->fs.mode = ipc_file->mode;

	cd->rate = ipc_file->rate;
	cd->channels = ipc_file->channels;
	cd->frame_fmt = ipc_file->frame_fmt;

	/* open file handle(s) depending on mode */
	switch (cd->fs.mode) {
	case FILE_READ:
		cd->fs.rfh = fopen(cd->fs.fn, "r");
		if (!cd->fs.rfh) {
			fprintf(stderr, "error: opening file %s\n", cd->fs.fn);
			free(cd);
			free(dev);
			return NULL;
		}
		break;
	case FILE_WRITE:
		cd->fs.wfh = fopen(cd->fs.fn, "w");
		if (!cd->fs.wfh) {
			fprintf(stderr, "error: opening file %s\n", cd->fs.fn);
			free(cd);
			free(dev);
			return NULL;
		}
		break;
	default:
		/* TODO: duplex mode */
		break;
	}

	cd->fs.reached_eof = 0;
	cd->fs.n = 0;

	dev->state = COMP_STATE_READY;

	return dev;
}

static void file_free(struct comp_dev *dev)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "file_free()");

	if (cd->fs.mode == FILE_READ)
		fclose(cd->fs.rfh);
	else
		fclose(cd->fs.wfh);

	free(cd->fs.fn);
	free(cd);
	free(dev);
}

static int file_verify_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "file_verify_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "file_verify_params() error: comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

/**
 * \brief Sets file component audio stream parameters.
 * \param[in,out] dev Volume base component device.
 * \param[in] params Audio (PCM) stream parameters (ignored for this component)
 * \return Error code.
 *
 * All done in prepare() since we need to know source and sink component params.
 */
static int file_params(struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	int err;

	comp_info(dev, "file_params()");

	err = file_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "file_params(): pcm params verification failed.");
		return -EINVAL;
	}

	return 0;
}

static int fr_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	return -EINVAL;
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
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "file_cmd()");
	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = fr_cmd(dev, cdata);
		break;
	default:
		break;
	}

	return ret;
}

/*
 * copy and process stream samples
 * returns the number of bytes copied
 */
static int file_copy(struct comp_dev *dev)
{
	struct comp_buffer *buffer;
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int snk_frames;
	int src_frames;
	int bytes = cd->sample_container_bytes;
	int ret = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* file component sink buffer */
		buffer = list_first_item(&dev->bsink_list, struct comp_buffer,
					 source_list);

		/* test sink has enough free frames */
		snk_frames = audio_stream_get_free_frames(&buffer->stream);
		if (snk_frames > 0 && !cd->fs.reached_eof) {
			/* read PCM samples from file */
			ret = cd->file_func(dev, &buffer->stream, NULL,
					    snk_frames);

			/* update sink buffer pointers */
			if (ret > 0)
				comp_update_buffer_produce(buffer,
							   ret * bytes);
		}
		break;
	case FILE_WRITE:
		/* file component source buffer */
		buffer = list_first_item(&dev->bsource_list,
					 struct comp_buffer, sink_list);

		/* test source has enough free frames */
		src_frames = audio_stream_get_avail_frames(&buffer->stream);
		if (src_frames > 0) {
			/* write PCM samples into file */
			ret = cd->file_func(dev, NULL, &buffer->stream,
					    src_frames);

			/* update source buffer pointers */
			if (ret > 0)
				comp_update_buffer_consume(buffer,
							   ret * bytes);
		}
		break;
	default:
		/* TODO: duplex mode */
		break;
	}

	return ret;
}

static int file_prepare(struct comp_dev *dev)
{
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct comp_buffer *buffer = NULL;
	struct file_comp_data *cd = comp_get_drvdata(dev);
	struct audio_stream *stream;
	int periods;
	int ret = 0;

	comp_info(dev, "file_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* file component source or sink buffer */
	if (cd->fs.mode == FILE_WRITE) {
		stream = &list_first_item(&dev->bsource_list,
					 struct comp_buffer, sink_list)->stream;
	} else {
		stream = &list_first_item(&dev->bsink_list, struct comp_buffer,
					 source_list)->stream;
	}

	if (stream->frame_fmt == SOF_IPC_FRAME_S16_LE)
		cd->sample_container_bytes = 2;
	else
		cd->sample_container_bytes = 4;

	/* calculate period size based on config */
	cd->period_bytes = dev->frames * cd->sample_container_bytes *
		stream->channels;

	/* file component sink/source buffer period count */
	switch (cd->fs.mode) {
	case FILE_READ:
		buffer = list_first_item(&dev->bsink_list, struct comp_buffer,
					 source_list);
		periods = config->periods_sink;
		break;
	case FILE_WRITE:
		buffer = list_first_item(&dev->bsource_list,
					 struct comp_buffer, sink_list);
		periods = config->periods_source;
		break;
	default:
		/* TODO: duplex mode */
		break;
	}

	if (!buffer) {
		fprintf(stderr, "error: no sink/source buffer\n");
		return -EINVAL;
	}

	/* set downstream buffer size */
	switch (config->frame_fmt) {
	case(SOF_IPC_FRAME_S16_LE):
		ret = buffer_set_size(buffer, dev->frames * 2 *
			periods * buffer->stream.channels);
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}
		buffer_reset_pos(buffer, NULL);

		/* set file function */
		cd->file_func = file_s16;
		break;
	case(SOF_IPC_FRAME_S24_4LE):
		ret = buffer_set_size(buffer, dev->frames * 4 *
			periods * buffer->stream.channels);
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}
		buffer_reset_pos(buffer, NULL);

		/* set file function */
		cd->file_func = file_s24;
		break;
	case(SOF_IPC_FRAME_S32_LE):
		ret = buffer_set_size(buffer, dev->frames * 4 *
			periods * buffer->stream.channels);
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}
		buffer_reset_pos(buffer, NULL);
		break;
	default:
		return -EINVAL;
	}

	dev->state = COMP_STATE_PREPARE;

	return ret;
}

static int file_reset(struct comp_dev *dev)
{
	comp_info(dev, "file_reset()");
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static int file_get_hw_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params, int dir)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "file_hw_params()");
	params->direction = dir;
	params->rate = cd->rate;
	params->channels = cd->channels;
	params->buffer_fmt = 0;
	params->frame_fmt = cd->frame_fmt;
	return 0;
}

static const struct comp_driver comp_file_host = {
	.type = SOF_COMP_HOST,
	.uid	= SOF_UUID(file_tr),
	.tctx	= &file_tr,
	.ops = {
		.create = file_new,
		.free = file_free,
		.params = file_params,
		.cmd = file_cmd,
		.trigger = file_trigger,
		.copy = file_copy,
		.prepare = file_prepare,
		.reset = file_reset,
	},

};

static const struct comp_driver comp_file_dai = {
	.type = SOF_COMP_DAI,
	.uid	= SOF_UUID(file_tr),
	.tctx	= &file_tr,
	.ops = {
		.create = file_new,
		.free = file_free,
		.params = file_params,
		.cmd = file_cmd,
		.trigger = file_trigger,
		.copy = file_copy,
		.prepare = file_prepare,
		.reset = file_reset,
		.dai_get_hw_params = file_get_hw_params,
	},
};

static struct comp_driver_info comp_file_host_info = {
	.drv = &comp_file_host,
};

static struct comp_driver_info comp_file_dai_info = {
	.drv = &comp_file_dai,
};

void sys_comp_file_init(void)
{
	comp_register(&comp_file_host_info);
	comp_register(&comp_file_dai_info);
}

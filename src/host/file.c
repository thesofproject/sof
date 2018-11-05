/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author(s): Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

/* file component for reading/writing pcm samples to/from a file */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <inttypes.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/work.h>
#include <sof/clk.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <uapi/ipc/stream.h>
#include "host/common_test.h"
#include "host/file.h"

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
static int read_samples_32(struct comp_dev *dev, struct comp_buffer *sink,
			   int n, int fmt, int nch)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t sample;
	int n_samples = 0;
	int i, n_wrap, n_min, ret;

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
static int read_samples_16(struct comp_dev *dev, struct comp_buffer *sink,
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
				/* read sample from file */
				ret = fscanf(cd->fs.rfh, "%hd", dest);
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
static int write_samples_16(struct comp_dev *dev, struct comp_buffer *source,
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
static int write_samples_32(struct comp_dev *dev, struct comp_buffer *source,
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
static int file_s32_default(struct comp_dev *dev, struct comp_buffer *sink,
			    struct comp_buffer *source, uint32_t frames)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int nch = dev->params.channels;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		n_samples = read_samples_32(dev, sink, frames * nch,
					    SOF_IPC_FRAME_S32_LE, nch);
		break;
	case FILE_WRITE:
		/* write samples */
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
static int file_s16(struct comp_dev *dev, struct comp_buffer *sink,
		    struct comp_buffer *source, uint32_t frames)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int nch = dev->params.channels;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		n_samples = read_samples_16(dev, sink, frames * nch, nch);
		break;
	case FILE_WRITE:
		/* write samples */
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
static int file_s24(struct comp_dev *dev, struct comp_buffer *sink,
		    struct comp_buffer *source, uint32_t frames)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int nch = dev->params.channels;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		n_samples = read_samples_32(dev, sink, frames * nch,
					    SOF_IPC_FRAME_S24_4LE, nch);
		break;
	case FILE_WRITE:
		/* write samples */
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

static struct comp_dev *file_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_file *file;
	struct sof_ipc_comp_file *ipc_file =
		(struct sof_ipc_comp_file *)comp;
	struct file_comp_data *cd;

	/* allocate memory for file comp */
	dev = malloc(COMP_SIZE(struct sof_ipc_comp_file));
	if (!dev)
		return NULL;

	/* copy file comp config */
	file = (struct sof_ipc_comp_file *)&dev->comp;
	memcpy(file, ipc_file, sizeof(struct sof_ipc_comp_file));

	/* allocate  memory for file comp data */
	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
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

	if (cd->fs.mode == FILE_READ)
		fclose(cd->fs.rfh);
	else
		fclose(cd->fs.wfh);

	free(cd->fs.fn);
	free(cd);
	free(dev);

	debug_print("free file component\n");
}

/* set component audio stream parameters */
static int file_params(struct comp_dev *dev)
{
	struct file_comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);

	/* for file endpoint set the following from topology config */
	if (cd->fs.mode == FILE_WRITE) {
		dev->params.frame_fmt = config->frame_fmt;
		if (dev->params.frame_fmt == SOF_IPC_FRAME_S16_LE)
			dev->params.sample_container_bytes = 2;
		else
			dev->params.sample_container_bytes = 4;
	}

	/* Need to compute this in non-host endpoint */
	dev->frame_bytes =
		dev->params.sample_container_bytes * dev->params.channels;

	/* calculate period size based on config */
	cd->period_bytes = dev->frames * dev->frame_bytes;

	/* File to sink supports only S32_LE/S16_LE/S24_4LE PCM formats */
	if (config->frame_fmt != SOF_IPC_FRAME_S32_LE &&
	    config->frame_fmt != SOF_IPC_FRAME_S24_4LE &&
	    config->frame_fmt != SOF_IPC_FRAME_S16_LE)
		return -EINVAL;

	return 0;
}

static int fr_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	return -EINVAL;
}

static int file_trigger(struct comp_dev *dev, int cmd)
{
	return comp_set_state(dev, cmd);
}

/* used to pass standard and bespoke commands (with data) to component */
static int file_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

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
	int ret = 0, bytes;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* file component sink buffer */
		buffer = list_first_item(&dev->bsink_list, struct comp_buffer,
					 source_list);

		/* test sink has enough free frames */
		if (buffer->free >= cd->period_bytes && !cd->fs.reached_eof) {
			/* read PCM samples from file */
			ret = cd->file_func(dev, buffer, NULL, dev->frames);

			/* update sink buffer pointers */
			bytes = dev->params.sample_container_bytes;
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
		if (buffer->avail >= cd->period_bytes) {
			/* write PCM samples into file */
			ret = cd->file_func(dev, NULL, buffer, dev->frames);

			/* update source buffer pointers */
			bytes = dev->params.sample_container_bytes;
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
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_buffer *buffer = NULL;
	struct file_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0, periods;

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
			periods * dev->params.channels);
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}
		buffer_reset_pos(buffer);

		/* set file function */
		cd->file_func = file_s16;
		break;
	case(SOF_IPC_FRAME_S24_4LE):
		ret = buffer_set_size(buffer, dev->frames * 4 *
			periods * dev->params.channels);
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}
		buffer_reset_pos(buffer);

		/* set file function */
		cd->file_func = file_s24;
		break;
	case(SOF_IPC_FRAME_S32_LE):
		ret = buffer_set_size(buffer, dev->frames * 4 *
			periods * dev->params.channels);
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}
		buffer_reset_pos(buffer);
		break;
	default:
		return -EINVAL;
	}

	dev->state = COMP_STATE_PREPARE;

	return ret;
}

static int file_reset(struct comp_dev *dev)
{
	dev->state = COMP_STATE_INIT;

	return 0;
}

struct comp_driver comp_file = {
	.type = SOF_COMP_FILEREAD,
	.ops = {
		.new = file_new,
		.free = file_free,
		.params = file_params,
		.cmd = file_cmd,
		.trigger = file_trigger,
		.copy = file_copy,
		.prepare = file_prepare,
		.reset = file_reset,
	},
};

void sys_comp_file_init(void)
{
	comp_register(&comp_file);
}

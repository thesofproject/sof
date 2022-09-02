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
#include <sof/audio/ipc-config.h>
#include <rtos/clk.h>
#include <sof/ipc/driver.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <ipc/stream.h>
#include "testbench/common_test.h"
#include "testbench/file.h"

/* bfc7488c-75aa-4ce8-9bde-d8da08a698c2 */
DECLARE_SOF_RT_UUID("file", file_uuid, 0xbfc7488c, 0x75aa, 0x4ce8,
		    0x9d, 0xbe, 0xd8, 0xda, 0x08, 0xa6, 0x98, 0xc2);
DECLARE_TR_CTX(file_tr, SOF_UUID(file_uuid), LOG_LEVEL_INFO);

static const struct comp_driver comp_file_dai;
static const struct comp_driver comp_file_host;

/*
 * Helpers for s24_4le data. To avoid an overflown 24 bit to be taken as valid 32 bit
 * sample mask in file read the 8 most signing bits to zeros. In file write similarly
 * shift the 24 bit word to MSB and possibly overflow it, then shift it back to LSB
 * side to create sign extension.
 */

static void mask_sink_s24(const struct audio_stream *sink, int samples)
{
	int32_t *snk = (int32_t *)sink->w_ptr;
	size_t bytes = samples * sizeof(int32_t);
	size_t bytes_snk;
	int samples_avail;
	int i;

	while (bytes) {
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		samples_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_snk));
		for (i = 0; i < samples_avail; i++)
			*snk++ &= 0x00ffffff;

		bytes -= samples * sizeof(int32_t);
		snk = audio_stream_wrap(sink, snk);
	}
}

static void sign_extend_source_s24(const struct audio_stream *source, int samples)
{
	int32_t tmp;
	int32_t *src = (int32_t *)source->r_ptr;
	size_t bytes = samples * sizeof(int32_t);
	size_t bytes_src;
	int samples_avail;
	int i;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		samples_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_src));
		for (i = 0; i < samples_avail; i++) {
			tmp = *src << 8;
			*src++ = tmp >> 8;
		}

		bytes -= samples * sizeof(int32_t);
		src = audio_stream_wrap(source, src);
	}
}

/*
 * Read 32-bit samples from binary file
 */
static int read_binary_s32(struct file_comp_data *cd, const struct audio_stream *sink, int samples)
{
	int32_t *snk = (int32_t *)sink->w_ptr;
	size_t samples_avail;
	size_t bytes_snk;
	size_t bytes = samples * sizeof(int32_t);
	int ret;
	int samples_copied = 0;

	while (bytes) {
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		samples_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_snk));
		ret = fread(snk, sizeof(int32_t), samples_avail, cd->fs.rfh);
		if (!ret) {
			cd->fs.reached_eof = 1;
			return samples_copied;
		}

		samples_copied += ret;
		bytes -= ret * sizeof(int32_t);
		snk = audio_stream_wrap(sink, snk + ret);
	}

	return samples_copied;
}

/*
 * Write 32-bit samples to binary file
 */
static int write_binary_s32(struct file_comp_data *cd, const struct audio_stream *source,
			    int samples)
{
	int32_t *src = (int32_t *)source->r_ptr;
	size_t samples_avail;
	size_t bytes_src;
	size_t bytes = samples * sizeof(int32_t);
	int ret;
	int samples_copied = 0;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		samples_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_src));
		ret = fwrite(src, sizeof(int32_t), samples_avail, cd->fs.wfh);
		if (ret == 0) {
			cd->fs.write_failed = true;
			return samples_copied;
		}
		samples_copied += ret;
		bytes -= ret * sizeof(int32_t);
		src = audio_stream_wrap(source, src + ret);
	}

	return samples_copied;
}

/*
 * Read 32-bit samples from text file
 */
static int read_text_s32(struct file_comp_data *cd, const struct audio_stream *sink, int samples)
{
	int32_t *snk = (int32_t *)sink->w_ptr;
	size_t bytes = samples * sizeof(int32_t);
	size_t bytes_snk;
	int ret;
	int i;
	int samples_copied = 0;

	while (bytes) {
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		samples = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_snk));
		for (i = 0; i < samples; i++) {
			ret = fscanf(cd->fs.rfh, "%d", snk++);
			if (ret == EOF) {
				cd->fs.reached_eof = 1;
				return samples_copied;
			}
			samples_copied++;
			bytes -= sizeof(int32_t);
		}
		snk = audio_stream_wrap(sink, snk);
	}

	return samples_copied;
}

/*
 * Write 32-bit samples to text file
 */
static int write_text_s32(struct file_comp_data *cd, const struct audio_stream *source, int samples)
{
	int32_t *src = (int32_t *)source->r_ptr;
	size_t bytes = samples * sizeof(int32_t);
	size_t bytes_src;
	int ret;
	int i;
	int samples_copied = 0;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		samples = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_src));
		for (i = 0; i < samples; i++) {
			ret = fprintf(cd->fs.wfh, "%d\n", *src++);
			if (ret < 1) {
				cd->fs.write_failed = true;
				return samples_copied;
			}
			samples_copied++;
			bytes -= sizeof(int32_t);
		}
		src = audio_stream_wrap(source, src);
	}

	return samples_copied;
}

static int read_samples_s32(struct file_comp_data *cd, const struct audio_stream *sink,
			    int samples, int fmt)
{
	int n_samples = 0;

	switch (cd->fs.f_format) {
	case FILE_RAW:
		/* raw input file */
		n_samples = read_binary_s32(cd, sink, samples);
		break;
	case FILE_TEXT:
		/* text input file */
		n_samples = read_text_s32(cd, sink, samples);
		break;
	default:
		return -EINVAL;
	}

	if (fmt == SOF_IPC_FRAME_S24_4LE)
		mask_sink_s24(sink, samples);

	return n_samples;
}

/*
 * Write 32-bit samples to file
 */
static int write_samples_s32(struct file_comp_data *cd, struct audio_stream *source, int samples,
			     int fmt)
{
	int samples_written;

	if (fmt == SOF_IPC_FRAME_S24_4LE)
		sign_extend_source_s24(source, samples);

	switch (cd->fs.f_format) {
	case FILE_RAW:
		/* raw input file */
		samples_written = write_binary_s32(cd, source, samples);
		break;
	case FILE_TEXT:
		/* text input file */
		samples_written = write_text_s32(cd, source, samples);
		break;
	default:
		return -EINVAL;
	}

	return samples_written;
}

/*
 * Read 16-bit samples from binary file
 */
static int read_binary_s16(struct file_comp_data *cd, const struct audio_stream *sink, int samples)
{
	int16_t *snk = (int16_t *)sink->w_ptr;
	size_t samples_avail;
	size_t bytes_snk;
	size_t bytes = samples * sizeof(int16_t);
	int ret;
	int samples_copied = 0;

	while (bytes) {
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		samples_avail = FILE_BYTES_TO_S16_SAMPLES(MIN(bytes, bytes_snk));
		ret = fread(snk, sizeof(int16_t), samples_avail, cd->fs.rfh);
		if (!ret) {
			cd->fs.reached_eof = 1;
			return samples_copied;
		}

		samples_copied += ret;
		bytes -= ret * sizeof(int16_t);
		snk = audio_stream_wrap(sink, snk + ret);
	}

	return samples_copied;
}

/*
 * Write 16-bit samples to binary file
 */
static int write_binary_s16(struct file_comp_data *cd, const struct audio_stream *source,
			    int samples)
{
	int16_t *src = (int16_t *)source->r_ptr;
	size_t samples_avail;
	size_t bytes_src;
	size_t bytes = samples * sizeof(int16_t);
	int ret;
	int samples_copied = 0;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		samples_avail = FILE_BYTES_TO_S16_SAMPLES(MIN(bytes, bytes_src));
		ret = fwrite(src, sizeof(int16_t), samples_avail, cd->fs.wfh);
		if (!ret) {
			cd->fs.write_failed = true;
			return samples_copied;
		}

		samples_copied += ret;
		bytes -= ret * sizeof(int16_t);
		src = audio_stream_wrap(source, src + ret);
	}

	return samples_copied;
}

/*
 * Read 16-bit samples from text file
 */
static int read_text_s16(struct file_comp_data *cd, const struct audio_stream *sink, int samples)
{
	int16_t *snk = (int16_t *)sink->w_ptr;
	size_t bytes = samples * sizeof(int16_t);
	size_t bytes_snk;
	int ret;
	int i;
	int samples_copied = 0;

	while (bytes) {
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		samples = FILE_BYTES_TO_S16_SAMPLES(MIN(bytes, bytes_snk));
		for (i = 0; i < samples; i++) {
			ret = fscanf(cd->fs.rfh, "%hd", snk++);
			if (ret == EOF) {
				cd->fs.reached_eof = true;
				return samples_copied;
			}
			samples_copied++;
			bytes -= sizeof(int16_t);
		}
		snk = audio_stream_wrap(sink, snk);
	}

	return samples_copied;
}

/*
 * Write 16-bit samples to text file
 */
static int write_text_s16(struct file_comp_data *cd, const struct audio_stream *source, int samples)
{
	int16_t *src = (int16_t *)source->r_ptr;
	size_t bytes = samples * sizeof(int16_t);
	size_t bytes_src;
	int ret;
	int i;
	int samples_copied = 0;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		samples = FILE_BYTES_TO_S16_SAMPLES(MIN(bytes, bytes_src));
		for (i = 0; i < samples; i++) {
			ret = fprintf(cd->fs.wfh, "%hd\n", *src++);
			if (ret < 1) {
				cd->fs.write_failed = true;
				return samples_copied;
			}
			samples_copied++;
			bytes -= sizeof(int16_t);
		}
		src = audio_stream_wrap(source, src);
	}

	return samples_copied;
}

static int read_samples_s16(struct file_comp_data *cd, const struct audio_stream *sink, int samples)
{
	int n_samples = 0;

	switch (cd->fs.f_format) {
	case FILE_RAW:
		/* raw input file */
		n_samples = read_binary_s16(cd, sink, samples);
		break;
	case FILE_TEXT:
		/* text input file */
		n_samples = read_text_s16(cd, sink, samples);
		break;
	default:
		return -EINVAL;
	}

	return n_samples;
}

/*
 * Write 16-bit samples to file
 */
static int write_samples_s16(struct file_comp_data *cd, struct audio_stream *source, int samples)
{
	int samples_written;

	switch (cd->fs.f_format) {
	case FILE_RAW:
		/* raw input file */
		samples_written = write_binary_s16(cd, source, samples);
		break;
	case FILE_TEXT:
		/* text input file */
		samples_written = write_text_s16(cd, source, samples);
		break;
	default:
		return -EINVAL;
	}

	return samples_written;
}

/* Default file copy function, just return error if called */
static int file_default(struct comp_dev *dev, struct audio_stream *sink,
			struct audio_stream *source, uint32_t frames)
{
	return -EINVAL;
}

/* function for processing 32-bit samples */
static int file_s32(struct comp_dev *dev, struct audio_stream *sink,
		    struct audio_stream *source, uint32_t frames)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct file_comp_data *cd = comp_get_drvdata(dd->dai);
	int nch;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = sink->channels;
		n_samples = read_samples_s32(cd, sink, frames * nch, SOF_IPC_FRAME_S32_LE);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = source->channels;
		n_samples = write_samples_s32(cd, source, frames * nch, SOF_IPC_FRAME_S32_LE);
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		return -EINVAL;
	}

	/* update sample counter and check if we have a sample limit */
	cd->fs.n += n_samples;
	if (cd->max_samples && cd->fs.n >= cd->max_samples)
		cd->fs.reached_eof = 1;

	return n_samples;
}

/* function for processing 16-bit samples */
static int file_s16(struct comp_dev *dev, struct audio_stream *sink,
		    struct audio_stream *source, uint32_t frames)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct file_comp_data *cd = comp_get_drvdata(dd->dai);
	int nch;
	int n_samples;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = sink->channels;
		n_samples = read_samples_s16(cd, sink, frames * nch);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = source->channels;
		n_samples = write_samples_s16(cd, source, frames * nch);
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		return -EINVAL;
	}

	/* update sample counter and check if we have a sample limit */
	cd->fs.n += n_samples;
	if (cd->max_samples && cd->fs.n >= cd->max_samples)
		cd->fs.reached_eof = 1;

	return n_samples;
}

/* function for processing 24-bit samples */
static int file_s24(struct comp_dev *dev, struct audio_stream *sink,
		    struct audio_stream *source, uint32_t frames)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct file_comp_data *cd = comp_get_drvdata(dd->dai);
	int nch;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = sink->channels;
		n_samples = read_samples_s32(cd, sink, frames * nch, SOF_IPC_FRAME_S24_4LE);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = source->channels;
		n_samples = write_samples_s32(cd, source, frames * nch, SOF_IPC_FRAME_S24_4LE);
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		return -EINVAL;
	}

	/* update sample counter and check if we have a sample limit */
	cd->fs.n += n_samples;
	if (cd->max_samples && cd->fs.n >= cd->max_samples)
		cd->fs.reached_eof = 1;

	return n_samples;
}

static enum file_format get_file_format(char *filename)
{
	char *ext = strrchr(filename, '.');

	if (!ext)
		return FILE_RAW;

	if (!strcmp(ext, ".txt"))
		return FILE_TEXT;

	return FILE_RAW;
}

static struct comp_dev *file_new(const struct comp_driver *drv,
				 struct comp_ipc_config *config,
				 void *spec)
{
	const struct dai_driver *fdrv;
	struct comp_dev *dev;
	struct ipc_comp_file *ipc_file = spec;
	struct dai_data *dd;
	struct dai *fdai;
	struct file_comp_data *cd;

	debug_print("file_new()\n");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	/* allocate  memory for file comp data */
	dd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dd));
	if (!dd)
		goto error_skip_dd;

	fdai = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*fdai));
	if (!fdai)
		goto error_skip_dai;

	fdrv = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*fdrv));
	if (!fdrv)
		goto error_skip_drv;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto error_skip_cd;

	fdai->drv = fdrv;
	dd->dai = fdai;
	comp_set_drvdata(dev, dd);
	comp_set_drvdata(dd->dai, cd);

	/* default function for processing samples */
	cd->file_func = file_default;

	/* get filename from IPC and open file */
	cd->fs.fn = strdup(ipc_file->fn);

	/* set file format */
	cd->fs.f_format = get_file_format(cd->fs.fn);

	/* set file comp mode */
	cd->fs.mode = ipc_file->mode;

	cd->rate = ipc_file->rate;
	cd->channels = ipc_file->channels;
	cd->frame_fmt = ipc_file->frame_fmt;
	dev->direction = ipc_file->direction;

	/* open file handle(s) depending on mode */
	switch (cd->fs.mode) {
	case FILE_READ:
		cd->fs.rfh = fopen(cd->fs.fn, "r");
		if (!cd->fs.rfh) {
			fprintf(stderr, "error: opening file %s for reading - %s\n",
				cd->fs.fn, strerror(errno));
			goto error;
		}
		break;
	case FILE_WRITE:
		cd->fs.wfh = fopen(cd->fs.fn, "w+");
		if (!cd->fs.wfh) {
			fprintf(stderr, "error: opening file %s for writing - %s\n",
				cd->fs.fn, strerror(errno));
			goto error;
		}
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		goto error;
	}

	cd->fs.reached_eof = false;
	cd->fs.write_failed = false;
	cd->fs.n = 0;
	cd->fs.copy_count = 0;
	dev->state = COMP_STATE_READY;
	return dev;

error:
	free(cd);

error_skip_cd:
	free((void *)fdrv);

error_skip_drv:
	free(fdai);

error_skip_dai:
	free(dd);

error_skip_dd:
	free(dev);
	return NULL;
}

static void file_free(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct file_comp_data *cd = comp_get_drvdata(dd->dai);

	comp_dbg(dev, "file_free()");

	if (cd->fs.mode == FILE_READ)
		fclose(cd->fs.rfh);
	else
		fclose(cd->fs.wfh);

	free(cd->fs.fn);
	free(cd);
	free((void *)dd->dai->drv);
	free(dd->dai);
	free(dd);
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
	struct comp_buffer *buffer;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct file_comp_data *cd = comp_get_drvdata(dd->dai);
	struct audio_stream *stream;
	int periods;
	int samples;
	int ret;

	comp_info(dev, "file_params()");

	ret = file_verify_params(dev, params);
	if (ret < 0) {
		comp_err(dev, "file_params(): pcm params verification failed.");
		return ret;
	}

	/* file component sink/source buffer period count */
	switch (cd->fs.mode) {
	case FILE_READ:
		buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		periods = dev->ipc_config.periods_sink;
		break;
	case FILE_WRITE:
		buffer = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		periods = dev->ipc_config.periods_source;
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		return -EINVAL;
	}

	/* set downstream buffer size */
	stream = &buffer->stream;
	samples = periods * dev->frames * stream->channels;
	switch (stream->frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		ret = buffer_set_size(buffer, samples * sizeof(int16_t));
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}

		/* set file function */
		cd->file_func = file_s16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		ret = buffer_set_size(buffer, samples * sizeof(int32_t));
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}

		/* set file function */
		cd->file_func = file_s24;
		break;
	case SOF_IPC_FRAME_S32_LE:
		ret = buffer_set_size(buffer, samples * sizeof(int32_t));
		if (ret < 0) {
			fprintf(stderr, "error: file buffer size set\n");
			return ret;
		}

		/* set file function */
		cd->file_func = file_s32;
		break;
	default:
		fprintf(stderr, "Warning: Unknown file sample format %d\n",
			dev->ipc_config.frame_fmt);
		return -EINVAL;
	}

	cd->sample_container_bytes = get_sample_bytes(stream->frame_fmt);
	buffer_reset_pos(buffer, NULL);

	return 0;
}

static int fr_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	fprintf(stderr, "Warning: Set data is not implemented\n");
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
		fprintf(stderr, "Warning: Unknown file command %d\n", cmd);
		return -EINVAL;
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
	struct dai_data *dd = comp_get_drvdata(dev);
	struct file_comp_data *cd = comp_get_drvdata(dd->dai);
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
		snk_frames = MIN(audio_stream_get_free_frames(&buffer->stream), dev->frames);
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
		ret = -EINVAL;
		break;
	}

	cd->fs.copy_count++;
	if (cd->fs.reached_eof || (cd->max_copies && cd->fs.copy_count >= cd->max_copies)) {
		cd->fs.reached_eof = 1;
		comp_info(dev, "file_copy(): copies %d max %d eof %d",
			  cd->fs.copy_count, cd->max_copies,
			  cd->fs.reached_eof);
		schedule_task_cancel(dev->pipeline->pipe_task);
	}
	return ret;
}

static int file_prepare(struct comp_dev *dev)
{
	int ret = 0;

	comp_info(dev, "file_prepare()");

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
	comp_info(dev, "file_reset()");
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static int file_get_hw_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params, int dir)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct file_comp_data *cd = comp_get_drvdata(dd->dai);

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
	.uid = SOF_RT_UUID(file_uuid),
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
	.uid = SOF_RT_UUID(file_uuid),
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

// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.

/* file component for reading/writing pcm samples to/from a file */

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/stream.h>
#include <sof/lib/dai.h>
#include <sof/ipc/driver.h>
#include <ipc/stream.h>
#include <rtos/init.h>
#include <rtos/clk.h>
#include <rtos/sof.h>
#include <sof/list.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "testbench/utils.h"
#include "testbench/file.h"
#include "testbench/file_ipc4.h"
#include "../../src/audio/copier/copier.h"

SOF_DEFINE_REG_UUID(file);
DECLARE_TR_CTX(file_tr, SOF_UUID(file_uuid), LOG_LEVEL_INFO);
LOG_MODULE_REGISTER(file, CONFIG_SOF_LOG_LEVEL);

/*
 * Helpers for s24_4le data. To avoid an overflown 24 bit to be taken as valid 32 bit
 * sample mask in file read the 8 most signing bits to zeros. In file write similarly
 * shift the 24 bit word to MSB and possibly overflow it, then shift it back to LSB
 * side to create sign extension.
 */

/* Size of the internal buffer used to sign extend s24_4le samples before a binary write */
#define FILE_S24_BUF_SAMPLES 256

static void mask_sink_s24(struct cir_buf_sink *sink, size_t samples)
{
	int32_t *snk = sink->ptr;
	size_t bytes = samples * sizeof(int32_t);
	size_t bytes_snk;
	size_t samples_avail;
	size_t i;

	while (bytes) {
		bytes_snk = cir_buf_bytes_without_wrap(snk, sink->buf_end);
		samples_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_snk));
		for (i = 0; i < samples_avail; i++)
			*snk++ &= 0x00ffffff;

		bytes -= samples * sizeof(int32_t);
		snk = cir_buf_wrap(snk, sink->buf_start, sink->buf_end);
	}
}

static inline int32_t s24_sign_extend(int32_t sample)
{
	return (int32_t)((uint32_t)sample << 8) >> 8;
}

/*
 * Read 32-bit samples from binary file
 */
static size_t read_binary_s32(struct file_comp_data *cd, struct cir_buf_sink *sink,
			      size_t samples)
{
	int32_t *snk = sink->ptr;
	size_t samples_avail;
	size_t bytes_snk;
	size_t bytes = samples * sizeof(int32_t);
	size_t ret;
	size_t samples_copied = 0;

	while (bytes) {
		bytes_snk = cir_buf_bytes_without_wrap(snk, sink->buf_end);
		samples_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_snk));
		ret = fread(snk, sizeof(int32_t), samples_avail, cd->fs.rfh);
		samples_copied += ret;
		if (ret < samples_avail) {
			cd->fs.reached_eof = 1;
			return samples_copied;
		}
		
		bytes -= ret * sizeof(int32_t);
		snk = cir_buf_wrap(snk + ret, sink->buf_start, sink->buf_end);
	}

	return samples_copied;
}

/*
 * Write 32-bit samples to binary file. The 24-bit samples in 32-bit containers are sign
 * extended in an internal buffer because the source data is read only.
 */
static size_t write_binary_s32(struct file_comp_data *cd, struct cir_buf_source *source,
			       size_t samples, int fmt)
{
	const int32_t *src = source->ptr;
	const int32_t *out;
	int32_t buf[FILE_S24_BUF_SAMPLES];
	size_t samples_avail;
	size_t bytes_src;
	size_t bytes = samples * sizeof(int32_t);
	size_t ret;
	size_t i;
	size_t samples_copied = 0;

	while (bytes) {
		bytes_src = cir_buf_bytes_without_wrap(src, source->buf_end);
		samples_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_src));
		out = src;
		if (fmt == SOF_IPC_FRAME_S24_4LE) {
			samples_avail = MIN(samples_avail, FILE_S24_BUF_SAMPLES);
			memcpy(buf, src, samples_avail * sizeof(int32_t));
			for (i = 0; i < samples_avail; i++)
				buf[i] = s24_sign_extend(buf[i]);

			out = buf;
		}

		ret = fwrite(out, sizeof(int32_t), samples_avail, cd->fs.wfh);
		samples_copied += ret;
		if (ret < samples_avail) {
			cd->fs.write_failed = true;
			return samples_copied;
		}

		bytes -= ret * sizeof(int32_t);
		src = source_cir_buf_wrap(src + ret, source->buf_start, source->buf_end);
	}

	return samples_copied;
}

/*
 * Read 32-bit samples from text file
 */
static size_t read_text_s32(struct file_comp_data *cd, struct cir_buf_sink *sink, size_t samples)
{
	int32_t *snk = sink->ptr;
	size_t bytes = samples * sizeof(int32_t);
	size_t bytes_snk;
	int ret;
	size_t i;
	size_t samples_copied = 0;

	while (bytes) {
		bytes_snk = cir_buf_bytes_without_wrap(snk, sink->buf_end);
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
		snk = cir_buf_wrap(snk, sink->buf_start, sink->buf_end);
	}

	return samples_copied;
}

/*
 * Write 32-bit samples to text file
 */
static size_t write_text_s32(struct file_comp_data *cd, struct cir_buf_source *source,
			     size_t samples, int fmt)
{
	const int32_t *src = source->ptr;
	size_t bytes = samples * sizeof(int32_t);
	size_t bytes_src;
	int32_t sample;
	int ret;
	size_t i;
	size_t samples_copied = 0;

	while (bytes) {
		bytes_src = cir_buf_bytes_without_wrap(src, source->buf_end);
		samples = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_src));
		for (i = 0; i < samples; i++) {
			sample = *src++;
			if (fmt == SOF_IPC_FRAME_S24_4LE)
				sample = s24_sign_extend(sample);

			ret = fprintf(cd->fs.wfh, "%d\n", sample);
			if (ret < 1) {
				cd->fs.write_failed = true;
				return samples_copied;
			}
			samples_copied++;
			bytes -= sizeof(int32_t);
		}
		src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
	}

	return samples_copied;
}

static int read_samples_s32(struct file_comp_data *cd, struct cir_buf_sink *sink, size_t *samples,
			    int fmt)
{
	switch (cd->fs.f_format) {
	case FILE_RAW:
		/* raw input file */
		*samples = read_binary_s32(cd, sink, *samples);
		break;
	case FILE_TEXT:
		/* text input file */
		*samples = read_text_s32(cd, sink, *samples);
		break;
	default:
		*samples = 0;
		return -EINVAL;
	}

	if (fmt == SOF_IPC_FRAME_S24_4LE)
		mask_sink_s24(sink, *samples);

	return 0;
}

/*
 * Write 32-bit samples to file
 */
static int write_samples_s32(struct file_comp_data *cd, struct cir_buf_source *source,
			     size_t *samples, int fmt)
{
	switch (cd->fs.f_format) {
	case FILE_RAW:
		/* raw input file */
		*samples = write_binary_s32(cd, source, *samples, fmt);
		break;
	case FILE_TEXT:
		/* text input file */
		*samples = write_text_s32(cd, source, *samples, fmt);
		break;
	default:
		*samples = 0;
		return -EINVAL;
	}

	return 0;
}

/*
 * Read 16-bit samples from binary file
 */
static size_t read_binary_s16(struct file_comp_data *cd, struct cir_buf_sink *sink,
			      size_t samples)
{
	int16_t *snk = sink->ptr;
	size_t samples_avail;
	size_t bytes_snk;
	size_t bytes = samples * sizeof(int16_t);
	size_t ret;
	size_t samples_copied = 0;

	while (bytes) {
		bytes_snk = cir_buf_bytes_without_wrap(snk, sink->buf_end);
		samples_avail = FILE_BYTES_TO_S16_SAMPLES(MIN(bytes, bytes_snk));
		ret = fread(snk, sizeof(int16_t), samples_avail, cd->fs.rfh);
		samples_copied += ret;
		if (ret < samples_avail) {
			cd->fs.reached_eof = 1;
			return samples_copied;
		}

		bytes -= ret * sizeof(int16_t);
		snk = cir_buf_wrap(snk + ret, sink->buf_start, sink->buf_end);
	}

	return samples_copied;
}

/*
 * Write 16-bit samples to binary file
 */
static size_t write_binary_s16(struct file_comp_data *cd, struct cir_buf_source *source,
			       size_t samples)
{
	const int16_t *src = source->ptr;
	size_t samples_avail;
	size_t bytes_src;
	size_t bytes = samples * sizeof(int16_t);
	size_t ret;
	size_t samples_copied = 0;

	while (bytes) {
		bytes_src = cir_buf_bytes_without_wrap(src, source->buf_end);
		samples_avail = FILE_BYTES_TO_S16_SAMPLES(MIN(bytes, bytes_src));
		ret = fwrite(src, sizeof(int16_t), samples_avail, cd->fs.wfh);
		if (!ret) {
			cd->fs.write_failed = true;
			return samples_copied;
		}

		samples_copied += ret;
		bytes -= ret * sizeof(int16_t);
		src = source_cir_buf_wrap(src + ret, source->buf_start, source->buf_end);
	}

	return samples_copied;
}

/*
 * Read 16-bit samples from text file
 */
static size_t read_text_s16(struct file_comp_data *cd, struct cir_buf_sink *sink, size_t samples)
{
	int16_t *snk = sink->ptr;
	size_t bytes = samples * sizeof(int16_t);
	size_t bytes_snk;
	int ret;
	size_t i;
	size_t samples_copied = 0;

	while (bytes) {
		bytes_snk = cir_buf_bytes_without_wrap(snk, sink->buf_end);
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
		snk = cir_buf_wrap(snk, sink->buf_start, sink->buf_end);
	}

	return samples_copied;
}

/*
 * Write 16-bit samples to text file
 */
static size_t write_text_s16(struct file_comp_data *cd, struct cir_buf_source *source,
			     size_t samples)
{
	const int16_t *src = source->ptr;
	size_t bytes = samples * sizeof(int16_t);
	size_t bytes_src;
	int ret;
	size_t i;
	size_t samples_copied = 0;

	while (bytes) {
		bytes_src = cir_buf_bytes_without_wrap(src, source->buf_end);
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
		src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
	}

	return samples_copied;
}

static int read_samples_s16(struct file_comp_data *cd, struct cir_buf_sink *sink, size_t *samples)
{
	switch (cd->fs.f_format) {
	case FILE_RAW:
		/* raw input file */
		*samples = read_binary_s16(cd, sink, *samples);
		break;
	case FILE_TEXT:
		/* text input file */
		*samples = read_text_s16(cd, sink, *samples);
		break;
	default:
		*samples = 0;
		return -EINVAL;
	}

	return 0;
}

/*
 * Write 16-bit samples to file
 */
static int write_samples_s16(struct file_comp_data *cd, struct cir_buf_source *source,
			     size_t *samples)
{
	switch (cd->fs.f_format) {
	case FILE_RAW:
		/* raw input file */
		*samples = write_binary_s16(cd, source, *samples);
		break;
	case FILE_TEXT:
		/* text input file */
		*samples = write_text_s16(cd, source, *samples);
		break;
	default:
		*samples = 0;
		return -EINVAL;
	}

	return 0;
}

/* Default file copy function, just return error if called */
static int file_default(struct file_comp_data *cd, struct cir_buf_sink *sink,
			struct cir_buf_source *source, size_t *samples)
{
	return -EINVAL;
}

/* function for processing 32-bit samples */
static int file_s32(struct file_comp_data *cd, struct cir_buf_sink *sink,
		    struct cir_buf_source *source, size_t *samples)
{
	int ret;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		ret = read_samples_s32(cd, sink, samples, SOF_IPC_FRAME_S32_LE);
		break;
	case FILE_WRITE:
		/* write samples */
		ret = write_samples_s32(cd, source, samples, SOF_IPC_FRAME_S32_LE);
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		*samples = 0;
		return -EINVAL;
	}

	if (ret)
		return ret;

	/* update sample counter and check if we have a sample limit */
	cd->fs.n += *samples;
	if (cd->max_samples && cd->fs.n >= cd->max_samples)
		cd->fs.reached_eof = 1;

	return 0;
}

/* function for processing 16-bit samples */
static int file_s16(struct file_comp_data *cd, struct cir_buf_sink *sink,
		    struct cir_buf_source *source, size_t *samples)
{
	int ret;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		ret = read_samples_s16(cd, sink, samples);
		break;
	case FILE_WRITE:
		/* write samples */
		ret = write_samples_s16(cd, source, samples);
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		*samples = 0;
		return -EINVAL;
	}

	if (ret)
		return ret;

	/* update sample counter and check if we have a sample limit */
	cd->fs.n += *samples;
	if (cd->max_samples && cd->fs.n >= cd->max_samples)
		cd->fs.reached_eof = 1;

	return 0;
}

/* function for processing 24-bit samples */
static int file_s24(struct file_comp_data *cd, struct cir_buf_sink *sink,
		    struct cir_buf_source *source, size_t *samples)
{
	int ret;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		ret = read_samples_s32(cd, sink, samples, SOF_IPC_FRAME_S24_4LE);
		break;
	case FILE_WRITE:
		/* write samples */
		ret = write_samples_s32(cd, source, samples, SOF_IPC_FRAME_S24_4LE);
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		*samples = 0;
		return -EINVAL;
	}

	if (ret)
		return ret;

	/* update sample counter and check if we have a sample limit */
	cd->fs.n += *samples;
	if (cd->max_samples && cd->fs.n >= cd->max_samples)
		cd->fs.reached_eof = 1;

	return 0;
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

#if CONFIG_IPC_MAJOR_4
/* Minimal support for IPC4 pipeline_comp_trigger()'s dai_get_init_delay_ms() */
static int file_init_set_dai_data(struct processing_module *mod)
{
	struct dai_data *dd;
	struct copier_data *ccd = module_get_private_data(mod);

	dd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*dd));
	if (!dd)
		return -ENOMEM;

	/* Member dd->dai remains NULL. It's sufficient for dai_get_init_delay_ms().
	 * In such case the functions returns zero delay. Testbench currently has
	 * no use for the feature.
	 */
	ccd->dd[0] = dd;
	return 0;
}

static void file_free_dai_data(struct processing_module *mod)
{
	struct copier_data *ccd = module_get_private_data(mod);

	free(ccd->dd[0]);
}
#else
/* Minimal support for IPC3 pipeline_comp_trigger()'s dai_get_init_delay_ms() */
static int file_init_set_dai_data(struct processing_module *mod)
{
	struct dai_data *dd;
	struct comp_dev *dev = mod->dev;

	dd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*dd));
	if (!dd)
		return -ENOMEM;

	/* Member dd->dai remains NULL. It's sufficient for dai_get_init_delay_ms().
	 * In such case the functions returns zero delay. Testbench currently has
	 * no use for the feature.
	 */
	dev->priv_data = dd;
	return 0;
}

static void file_free_dai_data(struct processing_module *mod)
{
	struct dai_data *dd;
	struct comp_dev *dev = mod->dev;

	dd = comp_get_drvdata(dev);
	free(dd);
}
#endif

static int file_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	struct copier_data *ccd;
	struct file_comp_data *cd;
	int ret;

#if CONFIG_IPC_MAJOR_4
	const struct ipc4_file_module_cfg *module_cfg =
		(const struct ipc4_file_module_cfg *)mod_data->cfg.init_data;

	const struct ipc4_file_config *ipc_file = &module_cfg->config;
#else
	const struct ipc_comp_file *ipc_file =
		(const struct ipc_comp_file *)mod_data->cfg.init_data;
#endif

	tb_debug_print("file_init()\n");

	ccd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*ccd));
	if (!ccd)
		return -ENOMEM;

	mod_data->private = ccd;

	/* File component data is placed to copier's ipcgtw_data */
	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd) {
		free(ccd);
		return -ENOMEM;
	}

	file_set_comp_data(ccd, cd);

	/* default function for processing samples */
	cd->file_func = file_default;

	/* get filename from IPC and open file */
	if (ipc_file->fn) {
		cd->fs.fn = strdup(ipc_file->fn);
	} else {
		fprintf(stderr, "error: no filename set\n");
		goto error;
	}

	/* set file format */
	cd->fs.f_format = get_file_format(cd->fs.fn);

	/* set file comp mode */
	cd->fs.mode = ipc_file->mode;
	cd->fs.rate = ipc_file->rate;
	cd->fs.channels = ipc_file->channels;
	cd->frame_fmt = ipc_file->frame_fmt;
	dev->direction = ipc_file->direction;
	dev->direction_set = true;

	/* open file handle(s) depending on mode */
	switch (cd->fs.mode) {
	case FILE_READ:
		cd->fs.rfh = fopen(cd->fs.fn, "r");
		if (!cd->fs.rfh) {
			fprintf(stderr, "error: opening file %s for reading - %s\n",
				cd->fs.fn, strerror(errno));
			goto error;
		}

		/* Change to DAI type is needed to avoid uninitialized hw params in
		 * pipeline_params, A file host can be left as SOF_COMP_MODULE_ADAPTER
		 */
		if (dev->direction == SOF_IPC_STREAM_CAPTURE) {
			dev->ipc_config.type = SOF_COMP_DAI;
			ret = file_init_set_dai_data(mod);
			if (ret) {
				fprintf(stderr, "error: failed set dai data.\n");
				goto error;
			}
		}
		break;
	case FILE_WRITE:
		cd->fs.wfh = fopen(cd->fs.fn, "w+");
		if (!cd->fs.wfh) {
			fprintf(stderr, "error: opening file %s for writing - %s\n",
				cd->fs.fn, strerror(errno));
			goto error;
		}

		/* Change to DAI type is needed to avoid uninitialized hw params in
		 * pipeline_params. For capture the file write is the host so set
		 * SOF_COMP_HOST to skip sink check in module_adapter_prepare().
		 */
		if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
			dev->ipc_config.type = SOF_COMP_DAI;
			ret = file_init_set_dai_data(mod);
			if (ret) {
				fprintf(stderr, "error: failed set dai data.\n");
				goto error;
			}
		} else {
			dev->ipc_config.type = SOF_COMP_HOST;
		}

		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		goto error;
	}

	cd->fs.reached_eof = false;
	cd->fs.write_failed = false;
	cd->fs.copy_timeout = false;
	cd->fs.n = 0;
	cd->fs.copy_count = 0;
	cd->fs.cycles_count = 0;
	return 0;

error:
	free(cd);
	free(ccd);
	return -EINVAL;
}

static int file_free(struct processing_module *mod)
{
	struct copier_data *ccd = module_get_private_data(mod);
	struct file_comp_data *cd = get_file_comp_data(ccd);

	tb_debug_print("file_free()\n");

	if (cd->fs.mode == FILE_READ)
		fclose(cd->fs.rfh);
	else
		fclose(cd->fs.wfh);

	file_free_dai_data(mod);
	free(cd->fs.fn);
	free(cd);
	free(ccd);
	return 0;
}

/*
 * copy and process stream samples
 * returns the number of bytes copied
 */
static int file_process(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct file_comp_data *cd = get_file_comp_data(module_get_private_data(mod));
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;
	struct sof_source *source;
	struct sof_sink *sink;
	size_t buf_size;
	size_t frames;
	size_t samples = 0;
	uint64_t cycles0, cycles1;
	int ret = 0;

	if (cd->fs.reached_eof)
		return -ENODATA;

	/* Note: a SOF_COMP_DAI does not have sources and sinks of the unused direction set */
	tb_getcycles(&cycles0);
	switch (cd->fs.mode) {
	case FILE_READ:
		/* read PCM samples from file */
		sink = sinks[0];
		frames = sink_get_free_frames(sink);
		frames = MIN(frames, cd->max_frames);
		ret = sink_get_buffer(sink, frames * sink_get_frame_bytes(sink), &sink_buf.ptr,
				      &sink_buf.buf_start, &buf_size);
		if (ret)
			return ret;

		sink_buf.buf_end = (char *)sink_buf.buf_start + buf_size;
		samples = frames * sink_get_channels(sink);
		ret = cd->file_func(cd, &sink_buf, NULL, &samples);
		if (ret)
			samples = 0;

		sink_commit_buffer(sink, samples * get_sample_bytes(sink_get_frm_fmt(sink)));
		break;
	case FILE_WRITE:
		/* write PCM samples into file */
		source = sources[0];
		frames = source_get_data_frames_available(source);
		frames = MIN(frames, cd->max_frames);
		ret = source_get_data(source, frames * source_get_frame_bytes(source),
				      &source_buf.ptr, &source_buf.buf_start, &buf_size);
		if (ret)
			return ret;

		source_buf.buf_end = (const char *)source_buf.buf_start + buf_size;
		samples = frames * source_get_channels(source);
		ret = cd->file_func(cd, NULL, &source_buf, &samples);
		if (ret)
			samples = 0;

		source_release_data(source, samples * get_sample_bytes(source_get_frm_fmt(source)));
		break;
	default:
		/* TODO: duplex mode */
		ret = -EINVAL;
		break;
	}

	cd->fs.copy_count++;
	if (cd->fs.reached_eof || (cd->max_copies && cd->fs.copy_count >= cd->max_copies)) {
		cd->fs.reached_eof = true;
		tb_debug_print("file_process(): reached EOF\n");
	}

	if (samples) {
		cd->copies_timeout_count = 0;
	} else {
		cd->copies_timeout_count++;
		if (cd->copies_timeout_count == FILE_MAX_COPIES_TIMEOUT) {
			tb_debug_print("file_process(): copies_timeout reached\n");
			cd->fs.copy_timeout = true;
		}
	}

	tb_getcycles(&cycles1);
	cd->fs.cycles_count += cycles1 - cycles0;
	return ret;
}

static int file_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct file_comp_data *cd = get_file_comp_data(module_get_private_data(mod));
	enum sof_ipc_frame frm_fmt;

	tb_debug_print("file_prepare()\n");

	/* file component sink/source buffer period count */
	cd->max_frames = dev->frames;

	/*
	 * Real copier/DAI components call ipc4_update_sink/source_format() to populate
	 * valid_sample_fmt; the testbench file component is the host endpoint stub
	 * so propagate cd->frame_fmt to the buffer's valid format too.
	 */
	switch (cd->fs.mode) {
	case FILE_READ:
		sink_set_valid_fmt(sinks[0], cd->frame_fmt);
		frm_fmt = sink_get_frm_fmt(sinks[0]);
		break;
	case FILE_WRITE:
		source_set_valid_fmt(sources[0], cd->frame_fmt);
		frm_fmt = source_get_frm_fmt(sources[0]);
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		return -EINVAL;
	}

	/* set file function */
	switch (frm_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		cd->file_func = file_s16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		cd->file_func = file_s24;
		break;
	case SOF_IPC_FRAME_S32_LE:
		cd->file_func = file_s32;
		break;
	default:
		fprintf(stderr, "Warning: Unknown file sample format %d\n",
			dev->ipc_config.frame_fmt);
		return -EINVAL;
	}

	return 0;
}

static int file_reset(struct processing_module *mod)
{
	struct file_comp_data *cd = module_get_private_data(mod);

	tb_debug_print("file_reset()\n");
	cd->copies_timeout_count = 0;
	return 0;
}

static int file_trigger(struct comp_dev *dev, int cmd)
{
	tb_debug_print("file_trigger()\n");
	return comp_set_state(dev, cmd);
}

static int file_get_hw_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params, int dir)
{
	struct processing_module *mod = comp_mod(dev);
	struct file_comp_data *cd = get_file_comp_data(module_get_private_data(mod));

	tb_debug_print("file_hw_params()\n");
	params->direction = dir;
	params->rate = cd->fs.rate;
	params->channels = cd->fs.channels;
	params->buffer_fmt = 0;
	params->frame_fmt = cd->frame_fmt;
	return 0;
}

/* Needed for SOF_COMP_DAI */
static struct module_endpoint_ops file_endpoint_ops = {
	.dai_get_hw_params = file_get_hw_params,
	.trigger = file_trigger,
};

static const struct module_interface file_interface = {
	.init = file_init,
	.prepare = file_prepare,
	.process = file_process,
	.reset = file_reset,
	.free = file_free,
	.endpoint_ops = &file_endpoint_ops,
};

DECLARE_MODULE_ADAPTER(file_interface, file_uuid, file_tr);
SOF_MODULE_INIT(file, sys_comp_module_file_interface_init);

// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.

/* file component for reading/writing pcm samples to/from a file */

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/pipeline.h>
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
#include <math.h>
#include "testbench/utils.h"
#include "testbench/file.h"
#include "testbench/file_ipc4.h"
#include "testbench/remote_server.h"
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

static int generate_tone_s32(struct file_comp_data *cd, const struct audio_stream *sink,
			     int samples, uint32_t freq_hz, int fmt)
{
	int32_t *snk = (int32_t *)sink->w_ptr;
	int nch = cd->fs.channels > 0 ? cd->fs.channels : 2;
	int rate = cd->fs.rate > 0 ? cd->fs.rate : 48000;
	double step = 2.0 * M_PI * (double)freq_hz / (double)rate;
	double scale = (fmt == SOF_IPC_FRAME_S24_4LE) ? (16000.0 * 256.0) : (16000.0 * 65536.0);
	size_t bytes = samples * sizeof(int32_t);
	int copied = 0;

	while (bytes >= nch * sizeof(int32_t)) {
		size_t bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		int frames_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_snk)) / nch;
		int f, c;

		if (frames_avail <= 0)
			frames_avail = 1;
		for (f = 0; f < frames_avail && bytes >= nch * sizeof(int32_t); f++) {
			int32_t val = (int32_t)(scale * sin(cd->fs.tone_phase));

			cd->fs.tone_phase += step;
			if (cd->fs.tone_phase >= 2.0 * M_PI)
				cd->fs.tone_phase -= 2.0 * M_PI;
			for (c = 0; c < nch; c++) {
				*snk = val;
				snk = audio_stream_wrap(sink, snk + 1);
				bytes -= sizeof(int32_t);
				copied++;
			}
		}
	}
	return copied;
}

static int generate_tone_s16(struct file_comp_data *cd, const struct audio_stream *sink,
			     int samples, uint32_t freq_hz)
{
	int16_t *snk = (int16_t *)sink->w_ptr;
	int nch = cd->fs.channels > 0 ? cd->fs.channels : 2;
	int rate = cd->fs.rate > 0 ? cd->fs.rate : 48000;
	double step = 2.0 * M_PI * (double)freq_hz / (double)rate;
	size_t bytes = samples * sizeof(int16_t);
	int copied = 0;

	while (bytes >= nch * sizeof(int16_t)) {
		size_t bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		int frames_avail = FILE_BYTES_TO_S16_SAMPLES(MIN(bytes, bytes_snk)) / nch;
		int f, c;

		if (frames_avail <= 0)
			frames_avail = 1;
		for (f = 0; f < frames_avail && bytes >= nch * sizeof(int16_t); f++) {
			int16_t val = (int16_t)(16000.0 * sin(cd->fs.tone_phase));

			cd->fs.tone_phase += step;
			if (cd->fs.tone_phase >= 2.0 * M_PI)
				cd->fs.tone_phase -= 2.0 * M_PI;
			for (c = 0; c < nch; c++) {
				*snk = val;
				snk = audio_stream_wrap(sink, snk + 1);
				bytes -= sizeof(int16_t);
				copied++;
			}
		}
	}
	return copied;
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
	uint32_t tone_freq = 0;

	if (tb_remote_is_tone_active(&tone_freq))
		return generate_tone_s32(cd, sink, samples, tone_freq, cd->frame_fmt);

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
	int shift = (cd->frame_fmt == SOF_IPC_FRAME_S24_4LE) ? 8 : 16;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		samples_avail = FILE_BYTES_TO_S32_SAMPLES(MIN(bytes, bytes_src));
		ret = fwrite(src, sizeof(int32_t), samples_avail, cd->fs.wfh);
		if (ret == 0) {
			cd->fs.write_failed = true;
			return samples_copied;
		}
		tb_remote_record_output_samples_s32(src, ret, cd->fs.channels,
						    cd->fs.rate, shift);
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
	uint32_t tone_freq = 0;

	if (tb_remote_is_tone_active(&tone_freq))
		return generate_tone_s16(cd, sink, samples, tone_freq);

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
		tb_remote_record_output_samples_s16(src, ret, cd->fs.channels, cd->fs.rate);

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
static int file_default(struct file_comp_data *cd, struct audio_stream *sink,
			struct audio_stream *source, uint32_t frames)
{
	return -EINVAL;
}

/* function for processing 32-bit samples */
static int file_s32(struct file_comp_data *cd, struct audio_stream *sink,
		    struct audio_stream *source, uint32_t frames)
{
	int nch;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = audio_stream_get_channels(sink);
		n_samples = read_samples_s32(cd, sink, frames * nch, SOF_IPC_FRAME_S32_LE);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = audio_stream_get_channels(source);
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
static int file_s16(struct file_comp_data *cd, struct audio_stream *sink,
		    struct audio_stream *source, uint32_t frames)
{
	int nch;
	int n_samples;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = audio_stream_get_channels(sink);
		n_samples = read_samples_s16(cd, sink, frames * nch);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = audio_stream_get_channels(source);
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
static int file_s24(struct file_comp_data *cd, struct audio_stream *sink,
		    struct audio_stream *source, uint32_t frames)
{
	int nch;
	int n_samples = 0;

	switch (cd->fs.mode) {
	case FILE_READ:
		/* read samples */
		nch = audio_stream_get_channels(sink);
		n_samples = read_samples_s32(cd, sink, frames * nch, SOF_IPC_FRAME_S24_4LE);
		break;
	case FILE_WRITE:
		/* write samples */
		nch = audio_stream_get_channels(source);
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

	rfree(ccd->dd[0]);
	ccd->dd[0] = NULL;
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
	rfree(dd);
	dev->priv_data = NULL;
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
		rfree(ccd);
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
		mod->max_sinks = SOF_WIDGET_MAX_NUM_PINS;

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
	if (cd->fs.rfh)
		fclose(cd->fs.rfh);
	if (cd->fs.wfh)
		fclose(cd->fs.wfh);
	file_free_dai_data(mod);
	free(cd->fs.fn);
	rfree(cd);
	rfree(ccd);
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
	rfree(cd);
	rfree(ccd);
	return 0;
}

/*
 * copy and process stream samples
 * returns the number of bytes copied
 */
static int file_process(struct processing_module *mod,
			struct input_stream_buffer *input_buffers, int num_input_buffers,
			struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	struct file_comp_data *cd = get_file_comp_data(module_get_private_data(mod));
	struct audio_stream *source;
	struct audio_stream *sink;
	struct comp_buffer *buffer;
	uint32_t frames;
	uint64_t cycles0, cycles1;
	int samples = 0;
	int ret = 0;

	if (cd->fs.reached_eof) {
		uint32_t tone_freq = 0;

		if (tb_remote_is_tone_active(&tone_freq)) {
			cd->fs.reached_eof = false;
			cd->fs.copy_timeout = false;
			cd->copies_timeout_count = 0;
		} else {
			return 0;
		}
	}

	/* Note: a SOF_COMP_DAI does not have input_buffers and output buffers set */
	tb_getcycles(&cycles0);
	switch (cd->fs.mode) {
	case FILE_READ:
	{
		struct comp_buffer *next_buf;
		void *first_wptr;
		size_t bytes_produced;

		/* read PCM samples from file into first consumer buffer */
		buffer = comp_dev_get_first_data_consumer(dev);
		comp_dev_for_each_consumer(dev, next_buf)
			comp_buffer_shared_lock(next_buf);
		sink = &buffer->stream;
		frames = audio_stream_get_free_frames(sink);
		frames = MIN(frames, cd->max_frames);
		comp_dev_for_each_consumer(dev, next_buf) {
			if (next_buf == buffer)
				continue;
			frames = MIN(frames, audio_stream_get_free_frames(&next_buf->stream));
		}
		first_wptr = sink->w_ptr;
		samples = cd->file_func(cd, sink, NULL, frames);
		bytes_produced = audio_stream_sample_bytes(sink) * samples;

		/* replicate samples to any additional consumer buffers */
		comp_dev_for_each_consumer(dev, next_buf) {
			struct audio_stream *nsink = &next_buf->stream;
			uint8_t *src_ptr = first_wptr;
			uint8_t *dst_ptr = nsink->w_ptr;
			size_t rem = bytes_produced;

			if (next_buf == buffer)
				continue;

			while (rem > 0) {
				size_t src_chunk = audio_stream_bytes_without_wrap(sink, src_ptr);
				size_t dst_chunk = audio_stream_bytes_without_wrap(nsink, dst_ptr);
				size_t chunk = MIN(rem, MIN(src_chunk, dst_chunk));

				memcpy(dst_ptr, src_ptr, chunk);
				rem -= chunk;
				src_ptr = audio_stream_wrap(sink, src_ptr + chunk);
				dst_ptr = audio_stream_wrap(nsink, dst_ptr + chunk);
			}
			comp_update_buffer_produce(next_buf, bytes_produced);
		}
		comp_update_buffer_produce(buffer, bytes_produced);
		comp_dev_for_each_consumer(dev, next_buf)
			comp_buffer_shared_unlock(next_buf);
		break;
	}
	case FILE_WRITE:
		/* write PCM samples into file */
		buffer = comp_dev_get_first_data_producer(dev);
		comp_buffer_shared_lock(buffer);
		source = &buffer->stream;
		frames = audio_stream_get_avail_frames(source);
		frames = MIN(frames, cd->max_frames);
		samples = cd->file_func(cd, NULL, source, frames);
		comp_update_buffer_consume(buffer, audio_stream_sample_bytes(source) * samples);
		comp_buffer_shared_unlock(buffer);
		if (samples > 0 && cd->fs.wfh)
			fflush(cd->fs.wfh);
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
		cd->fs.copy_timeout = false;
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
	struct audio_stream *stream;
	struct comp_buffer *buffer;
	struct comp_dev *dev = mod->dev;
	struct file_comp_data *cd = get_file_comp_data(module_get_private_data(mod));

	tb_debug_print("file_prepare()\n");

	/* file component sink/source buffer period count */
	cd->max_frames = dev->frames;
	switch (cd->fs.mode) {
	case FILE_READ:
		comp_dev_for_each_consumer(dev, buffer)
			audio_stream_set_valid_fmt(&buffer->stream, cd->frame_fmt);
		buffer = comp_dev_get_first_data_consumer(dev);
		break;
	case FILE_WRITE:
		buffer = comp_dev_get_first_data_producer(dev);
		audio_stream_set_valid_fmt(&buffer->stream, cd->frame_fmt);
		break;
	default:
		/* TODO: duplex mode */
		fprintf(stderr, "Error: Unknown file mode %d\n", cd->fs.mode);
		return -EINVAL;
	}

	/* set file function */
	stream = &buffer->stream;

	switch (audio_stream_get_frm_fmt(stream)) {
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
	struct file_comp_data *cd = get_file_comp_data(module_get_private_data(mod));

	tb_debug_print("file_reset()\n");
	cd->copies_timeout_count = 0;
	cd->fs.reached_eof = false;
	cd->fs.write_failed = false;
	cd->fs.copy_timeout = false;
	cd->fs.n = 0;
	cd->fs.copy_count = 0;
	cd->fs.cycles_count = 0;
	cd->fs.tone_phase = 0.0;
	if (cd->fs.mode == FILE_READ && cd->fs.fn) {
		if (cd->fs.rfh)
			fclose(cd->fs.rfh);
		cd->fs.rfh = fopen(cd->fs.fn, "r");
	} else if (cd->fs.mode == FILE_WRITE && cd->fs.fn) {
		if (cd->fs.wfh)
			fflush(cd->fs.wfh);
	}
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
	.process_audio_stream = file_process,
	.reset = file_reset,
	.free = file_free,
	.endpoint_ops = &file_endpoint_ops,
};

DECLARE_MODULE_ADAPTER(file_interface, file_uuid, file_tr);
SOF_MODULE_INIT(file, sys_comp_module_file_interface_init);

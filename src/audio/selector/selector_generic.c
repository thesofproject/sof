// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Lech Betlej <lech.betlej@linux.intel.com>

/**
 * \file
 * \brief Audio channel selector / extractor - generic processing functions
 * \authors Lech Betlej <lech.betlej@linux.intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/selector.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(selector, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_IPC_MAJOR_3
#if CONFIG_FORMAT_S16LE
/**
 * \brief Channel selection for 16 bit, 1 channel data format.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 */
static int sel_s16le_1ch(struct comp_dev *dev, struct sof_sink *sink, struct sof_source *source,
			 size_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const unsigned int sel_channel = cd->config.sel_channel; /* 0 to nch - 1 */
	const size_t source_frame_bytes = source_get_frame_bytes(source);
	const unsigned int nch = source_get_channels(source);
	const int16_t *src, *src_start, *src_ch;
	int16_t *dst, *dst_start;
	int src_samples, dst_samples;
	size_t i, n, processed = 0;
	int ret;

	ret = source_get_data_s16(source, frames * source_frame_bytes, &src, &src_start,
				  &src_samples);
	if (ret)
		return ret;

	ret = sink_get_buffer_s16(sink, frames * sizeof(*dst), &dst, &dst_start, &dst_samples);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	while (processed < frames) {
		n = frames - processed;
		n = MIN(n, (cir_buf_samples_to_wrap_s16(src, src_start, src_samples) / nch));
		n = MIN(n, cir_buf_samples_to_wrap_s16(dst, dst_start, dst_samples));
		src_ch = src + sel_channel;
		for (i = 0; i < n; i++) {
			*dst = *src_ch;
			src_ch += nch;
			dst++;
		}

		src += n * nch;
		if (src >= src_start + src_samples)
			src = src_start;
		if (dst >= dst_start + dst_samples)
			dst = dst_start;
		processed += n;
	}

	return release_source_and_commit_sink(source, frames * source_frame_bytes,
		sink, frames * sizeof(*dst));
}

/**
 * \brief Channel selection for 16 bit, at least 2 channels data format.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 */
static int sel_s16le_nch(struct comp_dev *dev, struct sof_sink *sink,
			  struct sof_source *source, size_t frames)
{
	const size_t frame_bytes = source_get_frame_bytes(source);
	const unsigned int nch = source_get_channels(source);
	const int16_t *src, *src_start;
	int16_t *dst, *dst_start;
	int src_samples, dst_samples;
	size_t n, processed = 0;
	int ret;

	ret = source_get_data_s16(source, frames * frame_bytes, &src, &src_start, &src_samples);
	if (ret)
		return ret;

	ret = sink_get_buffer_s16(sink, frames * frame_bytes, &dst, &dst_start, &dst_samples);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	while (processed < frames) {
		n = frames - processed;
		n = MIN(n, (cir_buf_samples_to_wrap_s16(src, src_start, src_samples) / nch));
		n = MIN(n, (cir_buf_samples_to_wrap_s16(dst, dst_start, dst_samples) / nch));
		memcpy_s(dst, n * frame_bytes, src, n * frame_bytes);
		src += n * nch;
		dst += n * nch;
		if (src >= src_start + src_samples)
			src = src_start;
		if (dst >= dst_start + dst_samples)
			dst = dst_start;
		processed += n;
	}

	return release_source_and_commit_sink(source, frames * frame_bytes,
		sink, frames * frame_bytes);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
/**
 * \brief Channel selection for 32 bit, 1 channel data format.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 */
static int sel_s32le_1ch(struct comp_dev *dev, struct sof_sink *sink, struct sof_source *source,
			 size_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const unsigned int sel_channel = cd->config.sel_channel; /* 0 to nch - 1 */
	const size_t source_frame_bytes = source_get_frame_bytes(source);
	const unsigned int nch = source_get_channels(source);
	const int32_t *src, *src_ch, *src_start;
	int32_t *dst, *dst_start;
	int src_samples, dst_samples;
	size_t i, n, processed = 0;
	int ret;

	ret = source_get_data_s32(source, frames * source_frame_bytes, &src, &src_start,
				  &src_samples);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, frames * sizeof(int32_t), &dst, &dst_start, &dst_samples);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	while (processed < frames) {
		n = frames - processed;
		n = MIN(n, cir_buf_samples_to_wrap_s32(src, src_start, src_samples) / nch);
		n = MIN(n, cir_buf_samples_to_wrap_s32(dst, dst_start, dst_samples));
		src_ch = src + sel_channel;
		for (i = 0; i < n; i++) {
			*dst = *src_ch;
			src_ch += nch;
			dst++;
		}
		src += nch * n;
		if (src >= src_start + src_samples)
			src = src_start;
		if (dst >= dst_start + dst_samples)
			dst = dst_start;
		processed += n;
	}

	return release_source_and_commit_sink(source, frames * source_frame_bytes,
		sink, frames * source_frame_bytes);
}

/**
 * \brief Channel selection for 32 bit, at least 2 channels data format.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 */
static int sel_s32le_nch(struct comp_dev *dev, struct sof_sink *sink, struct sof_source *source,
			 size_t frames)
{
	const int frame_bytes = source_get_frame_bytes(source);
	const unsigned int nch = source_get_channels(source);
	const int32_t *src, *src_start;
	int32_t *dst, *dst_start;
	int src_samples, dst_samples;
	size_t n, processed = 0;
	int ret;

	ret = source_get_data_s32(source, frames * frame_bytes,
				  &src, &src_start, &src_samples);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, frames * frame_bytes,
				  &dst, &dst_start, &dst_samples);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	while (processed < frames) {
		n = frames - processed;
		n = MIN(n, cir_buf_samples_to_wrap_s32(src, src_start, src_samples) / nch);
		n = MIN(n, cir_buf_samples_to_wrap_s32(dst, dst_start, dst_samples) / nch);
		memcpy_s(dst, n * frame_bytes, src, n * frame_bytes);
		src += n * nch;
		dst += n * nch;
		if (src >= src_start + src_samples)
			src = src_start;
		if (dst >= dst_start + dst_samples)
			dst = dst_start;
		processed += n;
	}

	return release_source_and_commit_sink(source, frames * frame_bytes,
		sink, frames * frame_bytes);
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

#else
#if CONFIG_FORMAT_S16LE
/**
 * \brief Mixing routine for 16-bit, m channel input x n channel output single frame.
 * \param[out] dst Sink buffer.
 * \param[in] dst_channels Number of sink channels.
 * \param[in] src Source data.
 * \param[in] src_channels Number of source channels.
 * \param[in] coeffs_config IPC4 micsel config with Q10 coefficients.
 */
static void process_frame_s16le(int16_t dst[], int dst_channels,
				const int16_t src[], int src_channels,
				struct ipc4_selector_coeffs_config *coeffs_config)
{
	int32_t accum;
	int i, j;

	for (i = 0; i < dst_channels; i++) {
		accum = 0;
		for (j = 0; j < src_channels; j++)
			accum += (int32_t)src[j] * (int32_t)coeffs_config->coeffs[i][j];

		/* shift out 10 LSbits with rounding to get 16-bit result */
		dst[i] = sat_int16((accum + (1 << 9)) >> 10);
	}
}

/**
 * \brief Channel selection for 16-bit, m channel input x n channel output data format.
 * \param[in] mod Selector base module device.
 * \param[in] source Source circular buffer view.
 * \param[in,out] sink Sink circular buffer view.
 * \param[in] frames Number of frames to process.
 */
static void sel_s16le(struct processing_module *mod,
		      const struct cir_buf_source *source,
		      struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int src_channels = cd->config.in_channels_count;
	const unsigned int dst_channels = cd->config.out_channels_count;
	const unsigned int n_chan_source = MIN(SEL_SOURCE_CHANNELS_MAX, (int)src_channels);
	const unsigned int n_chan_sink = MIN(SEL_SINK_CHANNELS_MAX, (int)dst_channels);
	const int16_t *src = source->ptr;
	int16_t *dst = sink->ptr;
	size_t i, n, processed = 0;

	while (processed < frames) {
		n = frames - processed;
		n = MIN(n, cir_buf_samples_without_wrap_s16(src, source->buf_end) / src_channels);
		n = MIN(n, cir_buf_samples_without_wrap_s16(dst, sink->buf_end) / dst_channels);
		for (i = 0; i < n; i++) {
			process_frame_s16le(dst, n_chan_sink, src, n_chan_source,
					    &cd->coeffs_config);
			src += src_channels;
			dst += dst_channels;
		}
		src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		processed += n;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/**
 * \brief Mixing routine for 24-bit, m channel input x n channel output single frame.
 * \param[out] dst Sink buffer.
 * \param[in] dst_channels Number of sink channels.
 * \param[in] src Source data.
 * \param[in] src_channels Number of source channels.
 * \param[in] coeffs_config IPC4 micsel config with Q10 coefficients.
 */
static void process_frame_s24le(int32_t dst[], int dst_channels,
				const int32_t src[], int src_channels,
				struct ipc4_selector_coeffs_config *coeffs_config)
{
	int64_t accum;
	int i, j;

	for (i = 0; i < dst_channels; i++) {
		accum = 0;
		for (j = 0; j < src_channels; j++)
			accum += (int64_t)src[j] * (int64_t)coeffs_config->coeffs[i][j];

		/* accum is Q1.23 * Q6.10 --> Q7.33, shift right by 10 and
		 * saturate to get Q1.23.
		 */
		dst[i] = sat_int24((accum + (1 << 9)) >> 10);
	}
}

/**
 * \brief Channel selection for 24-bit, m channel input x n channel output data format.
 * \param[in] mod Selector base module device.
 * \param[in] source Source circular buffer view.
 * \param[in,out] sink Sink circular buffer view.
 * \param[in] frames Number of frames to process.
 */
static void sel_s24le(struct processing_module *mod,
		      const struct cir_buf_source *source,
		      struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int src_channels = cd->config.in_channels_count;
	const unsigned int dst_channels = cd->config.out_channels_count;
	const unsigned int n_chan_source = MIN(SEL_SOURCE_CHANNELS_MAX, (int)src_channels);
	const unsigned int n_chan_sink = MIN(SEL_SINK_CHANNELS_MAX, (int)dst_channels);
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t i, n, processed = 0;

	while (processed < frames) {
		n = frames - processed;
		n = MIN(n, cir_buf_samples_without_wrap_s32(src, source->buf_end) / src_channels);
		n = MIN(n, cir_buf_samples_without_wrap_s32(dst, sink->buf_end) / dst_channels);
		for (i = 0; i < n; i++) {
			process_frame_s24le(dst, n_chan_sink, src, n_chan_source,
					    &cd->coeffs_config);
			src += src_channels;
			dst += dst_channels;
		}
		src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		processed += n;
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief Mixing routine for 32-bit, m channel input x n channel output single frame.
 * \param[out] dst Sink buffer.
 * \param[in] dst_channels Number of sink channels.
 * \param[in] src Source data.
 * \param[in] src_channels Number of source channels.
 * \param[in] coeffs_config IPC4 micsel config with Q10 coefficients.
 */
static void process_frame_s32le(int32_t dst[], int dst_channels,
				const int32_t src[], int src_channels,
				struct ipc4_selector_coeffs_config *coeffs_config)
{
	int64_t accum;
	int i, j;

	for (i = 0; i < dst_channels; i++) {
		accum = 0;
		for (j = 0; j < src_channels; j++)
			accum += (int64_t)src[j] * (int64_t)coeffs_config->coeffs[i][j];

		/* shift out 10 LSbits with rounding to get 32-bit result */
		dst[i] = sat_int32((accum + (1 << 9)) >> 10);
	}
}

/**
 * \brief Channel selection for 32-bit, m channel input x n channel output data format.
 * \param[in] mod Selector base module device.
 * \param[in] source Source circular buffer view.
 * \param[in,out] sink Sink circular buffer view.
 * \param[in] frames Number of frames to process.
 */
static void sel_s32le(struct processing_module *mod,
		      const struct cir_buf_source *source,
		      struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int src_channels = cd->config.in_channels_count;
	const unsigned int dst_channels = cd->config.out_channels_count;
	const unsigned int n_chan_source = MIN(SEL_SOURCE_CHANNELS_MAX, src_channels);
	const unsigned int n_chan_sink = MIN(SEL_SINK_CHANNELS_MAX, dst_channels);
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t i, n, processed = 0;

	while (processed < frames) {
		n = frames - processed;
		n = MIN(n, cir_buf_samples_without_wrap_s32(src, source->buf_end) / src_channels);
		n = MIN(n, cir_buf_samples_without_wrap_s32(dst, sink->buf_end) / dst_channels);
		for (i = 0; i < n; i++) {
			process_frame_s32le(dst, n_chan_sink, src, n_chan_source,
					    &cd->coeffs_config);
			src += src_channels;
			dst += dst_channels;
		}
		src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		processed += n;
	}
}
#endif /* CONFIG_FORMAT_S32LE */
#endif

const struct comp_func_map func_table[] = {
#if CONFIG_IPC_MAJOR_3
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE, 1, sel_s16le_1ch},
	{SOF_IPC_FRAME_S16_LE, 2, sel_s16le_nch},
	{SOF_IPC_FRAME_S16_LE, 4, sel_s16le_nch},
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S24_4LE, 1, sel_s32le_1ch},
	{SOF_IPC_FRAME_S24_4LE, 2, sel_s32le_nch},
	{SOF_IPC_FRAME_S24_4LE, 4, sel_s32le_nch},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE, 1, sel_s32le_1ch},
	{SOF_IPC_FRAME_S32_LE, 2, sel_s32le_nch},
	{SOF_IPC_FRAME_S32_LE, 4, sel_s32le_nch},
#endif /* CONFIG_FORMAT_S32LE */
#else
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE, 0, sel_s16le},
#endif
#if CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S24_4LE, 0, sel_s24le},
#endif
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE, 0, sel_s32le},
#endif
#endif
};

#if CONFIG_IPC_MAJOR_3
sel_func sel_get_processing_function(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i;

	/* map the channel selection function for source and sink buffers */
	for (i = 0; i < ARRAY_SIZE(func_table); i++) {
		if (cd->source_format != func_table[i].source)
			continue;
		if (cd->config.out_channels_count != func_table[i].out_channels)
			continue;

		/* TODO: add additional criteria as needed */
		return func_table[i].sel_func;
	}

	return NULL;
}
#else
sel_func sel_get_processing_function(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	int i;

	/* map the channel selection function for source and sink buffers */
	for (i = 0; i < ARRAY_SIZE(func_table); i++) {
		if (cd->source_format != func_table[i].source)
			continue;

		/* TODO: add additional criteria as needed */
		return func_table[i].sel_func;
	}

	return NULL;
}
#endif

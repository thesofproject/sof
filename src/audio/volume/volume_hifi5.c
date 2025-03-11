// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

/**
 * \file
 * \brief Volume HiFi5 processing implementation without peak volume detection
 * \authors Andrula Song <andrula.song@intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(volume, CONFIG_SOF_LOG_LEVEL);

#include "volume.h"

#if SOF_USE_HIFI(5, VOLUME)

#if (!CONFIG_COMP_PEAK_VOL)

#include <xtensa/tie/xt_hifi5.h>

/**
 * \brief store volume gain 4 times for xtensa multi-way intrinsic operations.
 * Simultaneous processing 4 samples.
 * \param[in,out] cd Volume component private data.
 * \param[in] channels_count Number of channels to process.
 */
static void vol_store_gain(struct vol_data *cd, const int channels_count)
{
	int32_t i;

	for (i = 0; i < channels_count; i++) {
		cd->vol[i] = cd->volume[i];
		cd->vol[i + channels_count * 1] = cd->volume[i];
		cd->vol[i + channels_count * 2] = cd->volume[i];
		cd->vol[i + channels_count * 3] = cd->volume[i];
	}
	cd->copy_gain = false;
}

#if CONFIG_FORMAT_S24LE
/**
 * \brief HiFi5 enabled volume processing from 24/32 bit to 24/32 or 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused)
 */
static void vol_s24_to_s24_s32(struct processing_module *mod, struct input_stream_buffer *bsource,
			       struct output_stream_buffer *bsink, uint32_t frames,
			       uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	ae_int32x2 in_sample, in_sample1;
	ae_int32x2 out_sample, out_sample1;
	ae_int32x2 volume, volume1;
	ae_int32x4 *buf;
	ae_int32x4 *buf_end;
	int i, n, m;
	ae_int32x4 *vol;
	ae_valignx2 inu;
	ae_valignx2 outu = AE_ZALIGN128();
	ae_int32x4 *in = (ae_int32x4 *)audio_stream_wrap(source,
							 (char *)audio_stream_get_rptr(source)
							 + bsource->consumed);
	ae_int32x4 *out = (ae_int32x4 *)audio_stream_wrap(sink,
							  (char *)audio_stream_get_wptr(sink)
							  + bsink->size);
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_int32x4);
	int samples = channels_count * frames;

	/* to ensure the address is 16-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	if (cd->copy_gain)
		vol_store_gain(cd, channels_count);

	buf = (ae_int32x4 *)cd->vol;
	buf_end = (ae_int32x4 *)(cd->vol + channels_count * 4);
	vol = buf;
	/* Set buf who stores the volume gain data as circular buffer */
	AE_SETCBEGIN0(buf);
	AE_SETCEND0(buf_end);

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);

	while (samples) {
		m = audio_stream_samples_without_wrap_s24(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s24(sink, out);
		n = MIN(m, n);
		inu = AE_LA128_PP(in);
		/* process four continuous samples per iteration */
		for (i = 0; i < n; i += 4) {
			/* Load the volume value */
			AE_L32X2X2_XC(volume, volume1, vol, inc);

			/* Load the input sample */
			AE_LA32X2X2_IP(in_sample, in_sample1, inu, in);

			/* Multiply the input sample */
#if COMP_VOLUME_Q8_16
			AE_MULF2P32X4RS(out_sample, out_sample1,
					AE_SLAI32S(volume, 7), AE_SLAI32S(volume1, 7),
					AE_SLAI32(in_sample, 8), AE_SLAI32(in_sample1, 8));
			out_sample = AE_SLAI32S(out_sample, 8);
			out_sample1 = AE_SLAI32S(out_sample1, 8);
#elif COMP_VOLUME_Q1_23
			AE_MULF2P32X4RS(out_sample, out_sample1, volume, volume1,
					AE_SLAI32(in_sample, 8), AE_SLAI32(in_sample1, 8));
			out_sample = AE_SLAI32S(out_sample, 8);
			out_sample1 = AE_SLAI32S(out_sample1, 8);
#elif COMP_VOLUME_Q1_31
			AE_MULF2P32X4RS(out_sample, out_sample1, volume, volume1,
					AE_SLAI32(in_sample, 8), AE_SLAI32(in_sample1, 8));
#endif

			/* Shift for S24_LE */
			out_sample = AE_SRAI32(out_sample, 8);
			out_sample1 = AE_SRAI32(out_sample1, 8);

			/* Store the output sample */
			AE_SA32X2X2_IP(out_sample, out_sample1, outu, out);
		}
		AE_SA128POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}

/**
 * \brief HiFi5 enabled volume passthrough from 24/32 bit to 24/32 or 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused)
 */
static void vol_passthrough_s24_to_s24_s32(struct processing_module *mod,
					   struct input_stream_buffer *bsource,
					   struct output_stream_buffer *bsink, uint32_t frames,
					   uint32_t attenuation)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	ae_int32x2 in_sample, in_sample1;
	int i, n, m;
	ae_valignx2 inu;
	ae_valignx2  outu = AE_ZALIGN128();
	ae_int32x4 *in = (ae_int32x4 *)audio_stream_wrap(source,
							 (char *)audio_stream_get_rptr(source)
							 + bsource->consumed);
	ae_int32x4 *out = (ae_int32x4 *)audio_stream_wrap(sink,
							  (char *)audio_stream_get_wptr(sink)
							  + bsink->size);
	int samples = audio_stream_get_channels(sink) * frames;

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);

	while (samples) {
		m = audio_stream_samples_without_wrap_s24(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s24(sink, out);
		n = MIN(m, n);
		inu = AE_LA128_PP(in);
		/* process 4 continuous samples once */
		for (i = 0; i < n; i += 4) {
			/* Load the input sample */
			AE_LA32X2X2_IP(in_sample, in_sample1, inu, in);
			/* Store the output sample */
			AE_SA32X2X2_IP(in_sample, in_sample1, outu, out);
		}
		AE_SA128POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}

#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief HiFi5 enabled volume processing from 32 bit to 24/32 or 32 bit.
 * \param[in,out] mod Pointer to struct processing_module
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused)
 */
static void vol_s32_to_s24_s32(struct processing_module *mod, struct input_stream_buffer *bsource,
			       struct output_stream_buffer *bsink, uint32_t frames,
			       uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	ae_int32x2 in_sample, in_sample1;
	ae_int32x2 out_sample, out_sample1;
	ae_int32x2 volume, volume1;
	int i, n, m;
	ae_int32x4 *buf;
	ae_int32x4 *buf_end;
	ae_int32x4 *vol;
	ae_valignx2 inu;
	ae_valignx2 outu = AE_ZALIGN128();
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_int32x4);
	int samples = channels_count * frames;
	ae_int32x4 *in = (ae_int32x4 *)audio_stream_wrap(source,
							 (char *)audio_stream_get_rptr(source)
							 + bsource->consumed);
	ae_int32x4 *out = (ae_int32x4 *)audio_stream_wrap(sink,
							  (char *)audio_stream_get_wptr(sink)
							  + bsink->size);

	/** to ensure the address is 16-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	if (cd->copy_gain)
		vol_store_gain(cd, channels_count);

	buf = (ae_int32x4 *)cd->vol;
	buf_end = (ae_int32x4 *)(cd->vol + channels_count * 4);
	vol = buf;
	/* Set buf who stores the volume gain data as circular buffer */
	AE_SETCBEGIN0(buf);
	AE_SETCEND0(buf_end);

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);

	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(m, n);
		inu = AE_LA128_PP(in);
		/* process four continuous samples per iteration */
		for (i = 0; i < n; i += 4) {
			/* Load the volume value */
			AE_L32X2X2_XC(volume, volume1, vol, inc);

			/* Load the input sample */
			AE_LA32X2X2_IP(in_sample, in_sample1, inu, in);
#if COMP_VOLUME_Q1_31
			AE_MULF2P32X4RS(out_sample, out_sample1,
					volume, volume1,
					in_sample, in_sample1);
#else
			/* With Q1.31 x Q1.31 -> Q17.47 HiFi multiplications the result is
			 * Q8.16 x Q1.31 << 1 >> 16 -> Q9.32, shift left by 15 for Q17.47
			 * Q1.23 x Q1.31 << 1 >> 16 -> Q2.39, shift left by 8 for Q17.47
			 */
			ae_int64 mult0, mult1;

			AE_MULF32X2R_HH_LL(mult0, mult1, volume, in_sample);
			mult0 = AE_SLAI64(mult0, VOLUME_Q17_47_SHIFT);
			mult1 = AE_SLAI64(mult1, VOLUME_Q17_47_SHIFT);
			out_sample = AE_ROUND32X2F48SSYM(mult0, mult1);	/* Q2.47 -> Q1.31 */

			AE_MULF32X2R_HH_LL(mult0, mult1, volume1, in_sample1);
			mult0 = AE_SLAI64(mult0, VOLUME_Q17_47_SHIFT);
			mult1 = AE_SLAI64(mult1, VOLUME_Q17_47_SHIFT);
			out_sample1 = AE_ROUND32X2F48SSYM(mult0, mult1); /* Q2.47 -> Q1.31 */
#endif
			AE_SA32X2X2_IP(out_sample, out_sample1, outu, out);
		}
		AE_SA128POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}

/**
 * \brief HiFi5 enabled volume passthrough from 32 bit to 24/32 or 32 bit.
 * \param[in,out] mod Pointer to struct processing_module
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused)
 */
static void vol_passthrough_s32_to_s24_s32(struct processing_module *mod,
					   struct input_stream_buffer *bsource,
					   struct output_stream_buffer *bsink, uint32_t frames,
					   uint32_t attenuation)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	ae_int32x2 in_sample, in_sample1;
	int i, n, m;
	ae_valignx2 inu;
	ae_valignx2 outu = AE_ZALIGN128();
	const int channels_count = audio_stream_get_channels(sink);
	int samples = channels_count * frames;
	ae_int32x4 *in = (ae_int32x4 *)audio_stream_wrap(source,
							 (char *)audio_stream_get_rptr(source)
							 + bsource->consumed);
	ae_int32x4 *out = (ae_int32x4 *)audio_stream_wrap(sink,
							  (char *)audio_stream_get_wptr(sink)
							  + bsink->size);

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);
	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(m, n);
		inu = AE_LA128_PP(in);
		/* process four continuous samples per iteration */
		for (i = 0; i < n; i += 4) {
			/* Load the input sample */
			AE_LA32X2X2_IP(in_sample, in_sample1, inu, in);
			AE_SA32X2X2_IP(in_sample, in_sample1, outu, out);
		}
		AE_SA128POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
/**
 * \brief HiFi5 enabled volume processing from 16 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused)
 */
static void vol_s16_to_s16(struct processing_module *mod, struct input_stream_buffer *bsource,
			   struct output_stream_buffer *bsink, uint32_t frames,
			   uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	ae_int32x2 volume, volume1, volume2, volume3;
	ae_int32x2 out_temp, out_temp1;
	ae_int16x4 in_sample, in_sample1;
	ae_int16x4 out_sample, out_sample1;
	int i, n, m;
	ae_int32x4 *buf;
	ae_int32x4 *buf_end;
	ae_int32x4 *vol;
	ae_valignx2 inu;
	ae_valignx2 outu = AE_ZALIGN128();
	ae_int16x8 *in = (ae_int16x8 *)audio_stream_wrap(source,
							 (char *)audio_stream_get_rptr(source)
							 + bsource->consumed);
	ae_int16x8 *out = (ae_int16x8 *)audio_stream_wrap(sink,
							  (char *)audio_stream_get_wptr(sink)
							  + bsink->size);
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_int32x4);
	int samples = channels_count * frames;

	/** to ensure the address is 16-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	if (cd->copy_gain)
		vol_store_gain(cd, channels_count);

	buf = (ae_int32x4 *)cd->vol;
	buf_end = (ae_int32x4 *)(cd->vol + channels_count * 4);
	vol = buf;

	/* Set buf as circular buffer */
	AE_SETCBEGIN0(buf);
	AE_SETCEND0(buf_end);

	while (samples) {
		m = audio_stream_samples_without_wrap_s16(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(m, n);
		inu = AE_LA128_PP(in);
		for (i = 0; i < n; i += 8) {
			/* load 4x2 volume gain */
			AE_L32X2X2_XC(volume, volume1, vol, inc);

			AE_L32X2X2_XC(volume2, volume3, vol, inc);

			/* Load the input sample */
			AE_LA16X4X2_IP(in_sample, in_sample1, inu, in);

#if COMP_VOLUME_Q1_31
			AE_MULF2P32X16X4RS(out_temp, out_temp1, volume, volume1, in_sample);
			out_sample = AE_ROUND16X4F32SSYM(out_temp, out_temp1);
			AE_MULF2P32X16X4RS(out_temp, out_temp1, volume2, volume3, in_sample1);
			out_sample1 = AE_ROUND16X4F32SSYM(out_temp, out_temp1);
#else
#if COMP_VOLUME_Q8_16
			/* Shift Q8.16 to Q9.23
			 * No need to shift Q1.23, it is OK as such
			 */
			volume = AE_SLAI32S(volume, 7);
			volume1 = AE_SLAI32S(volume1, 7);
			volume2 = AE_SLAI32S(volume2, 7);
			volume3 = AE_SLAI32S(volume3, 7);
#endif

			AE_MULF2P32X16X4RS(out_temp, out_temp1, volume, volume1, in_sample);
			/* Q9.23 to Q1.31 */
			out_temp = AE_SLAI32S(out_temp, 8);
			out_temp1 = AE_SLAI32S(out_temp1, 8);
			out_sample = AE_ROUND16X4F32SSYM(out_temp, out_temp1);

			AE_MULF2P32X16X4RS(out_temp, out_temp1, volume2, volume3, in_sample1);
			/* Q9.23 to Q1.31 */
			out_temp = AE_SLAI32S(out_temp, 8);
			out_temp1 = AE_SLAI32S(out_temp1, 8);
			out_sample1 = AE_ROUND16X4F32SSYM(out_temp, out_temp1);
#endif

			/* store the output */
			AE_SA16X4X2_IP(out_sample, out_sample1, outu, out);
		}
		AE_SA128POS_FP(outu, out);
		samples -= n;
		bsource->consumed += VOL_S16_SAMPLES_TO_BYTES(n);
		bsink->size += VOL_S16_SAMPLES_TO_BYTES(n);
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}

/**
 * \brief HiFi5 enabled volume passthrough from 16 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused)
 */
static void vol_passthrough_s16_to_s16(struct processing_module *mod,
				       struct input_stream_buffer *bsource,
				       struct output_stream_buffer *bsink, uint32_t frames,
				       uint32_t attenuation)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	ae_int16x4 in_sample, in_sample1;
	int i, n, m;
	ae_valignx2 inu;
	ae_valignx2 outu = AE_ZALIGN128();
	ae_int16x8 *in = (ae_int16x8 *)audio_stream_wrap(source,
							 (char *)audio_stream_get_rptr(source)
							 + bsource->consumed);
	ae_int16x8 *out = (ae_int16x8 *)audio_stream_wrap(sink,
							  (char *)audio_stream_get_wptr(sink)
							  + bsink->size);
	const int channels_count = audio_stream_get_channels(sink);
	int samples = channels_count * frames;

	bsource->consumed += VOL_S16_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S16_SAMPLES_TO_BYTES(samples);
	while (samples) {
		m = audio_stream_samples_without_wrap_s16(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(m, n);
		inu = AE_LA128_PP(in);
		for (i = 0; i < n; i += 8) {
			/* Load the input sample */
			AE_LA16X4X2_IP(in_sample, in_sample1, inu, in);
			AE_SA16X4X2_IP(in_sample, in_sample1, outu, out);
		}
		AE_SA128POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}
#endif /* CONFIG_FORMAT_S16LE */
const struct comp_func_map volume_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, vol_s16_to_s16, vol_passthrough_s16_to_s16},
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, vol_s24_to_s24_s32, vol_passthrough_s24_to_s24_s32},
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, vol_s32_to_s24_s32, vol_passthrough_s32_to_s24_s32},
#endif
};

const size_t volume_func_count = ARRAY_SIZE(volume_func_map);
#endif
#endif

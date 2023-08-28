// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

/**
 * \file
 * \brief Volume HiFi4 processing implementation without peak volume detection
 * \authors Andrula Song <andrula.song@intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(volume_hifi4, CONFIG_SOF_LOG_LEVEL);

#include "volume.h"

#ifdef VOLUME_HIFI4

#if (!CONFIG_COMP_PEAK_VOL)

#include <xtensa/tie/xt_hifi4.h>

/**
 * \brief store volume gain 4 times for xtensa multi-way intrinsic operations.
 * Simultaneous processing 2 data.
 * \param[in,out] cd Volume component private data.
 * \param[in] channels_count Number of channels to process.
 */
static void vol_store_gain(struct vol_data *cd, const int channels_count)
{
	int32_t i;

	/* using for loop instead of memcpy_s(), because for loop costs less cycles */
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
 * \brief HiFi4 enabled volume processing from 24/32 bit to 24/32 or 32 bit.
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
	ae_f32x2 in_sample = AE_ZERO32();
	ae_f32x2 out_sample = AE_ZERO32();
	ae_f32x2 volume = AE_ZERO32();
	ae_f32x2 *buf;
	ae_f32x2 *buf_end;
	int i, n, m;
	ae_f32x2 *vol;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	ae_f32x2 *in = (ae_f32x2 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						     + bsource->consumed);
	ae_f32x2 *out = (ae_f32x2 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						      + bsink->size);
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32x2);
	int samples = channels_count * frames;

	/** to ensure the adsress is 8-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	if (cd->copy_gain)
		vol_store_gain(cd, channels_count);

	buf = (ae_f32x2 *)cd->vol;
	buf_end = (ae_f32x2 *)(cd->vol + channels_count * 2);
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
		inu = AE_LA64_PP(in);
		/* process two continuous sample data once */
		for (i = 0; i < n; i += 2) {
			/* Load the volume value */
			AE_L32X2_XC(volume, vol, inc);

			/* Load the input sample */
			AE_LA32X2_IP(in_sample, inu, in);

			/* Multiply the input sample */
#if COMP_VOLUME_Q8_16
			out_sample = AE_MULFP32X2RS(AE_SLAI32S(volume, 7), AE_SLAI32(in_sample, 8));
#elif COMP_VOLUME_Q1_23
			out_sample = AE_MULFP32X2RS(volume, AE_SLAI32(in_sample, 8));
#else
#error "Need CONFIG_COMP_VOLUME_Qx_y"
#endif

			/* Shift for S24_LE */
			out_sample = AE_SLAI32S(out_sample, 8);
			out_sample = AE_SRAI32(out_sample, 8);

			/* Store the output sample */
			AE_SA32X2_IP(out_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}

/**
 * \brief HiFi4 enabled volume passthrough from 24/32 bit to 24/32 or 32 bit.
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
	ae_f32x2 in_sample = AE_ZERO32();
	int i, n, m;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	ae_f32x2 *in = (ae_f32x2 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						     + bsource->consumed);
	ae_f32x2 *out = (ae_f32x2 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						      + bsink->size);
	int samples = audio_stream_get_channels(sink) * frames;

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);

	while (samples) {
		m = audio_stream_samples_without_wrap_s24(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s24(sink, out);
		n = MIN(m, n);
		inu = AE_LA64_PP(in);
		/* process two continuous sample data once */
		for (i = 0; i < n; i += 2) {
			/* Load the input sample */
			AE_LA32X2_IP(in_sample, inu, in);
			/* Store the output sample */
			AE_SA32X2_IP(in_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}

#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief HiFi4 enabled volume processing from 32 bit to 24/32 or 32 bit.
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
	ae_f32x2 in_sample = AE_ZERO32();
	ae_f32x2 out_sample = AE_ZERO32();
	ae_f32x2 volume = AE_ZERO32();
	int i, n, m;
	ae_f64 mult0;
	ae_f64 mult1;
	ae_f32x2 *buf;
	ae_f32x2 *buf_end;
	ae_f32x2 *vol;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32x2);
	int samples = channels_count * frames;
	ae_f32x2 *in = (ae_f32x2 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						     + bsource->consumed);
	ae_f32x2 *out = (ae_f32x2 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						      + bsink->size);

	/** to ensure the address is 8-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	if (cd->copy_gain)
		vol_store_gain(cd, channels_count);

	buf = (ae_f32x2 *)cd->vol;
	buf_end = (ae_f32x2 *)(cd->vol + channels_count * 2);
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
		inu = AE_LA64_PP(in);
		/* process two continuous sample data once */
		for (i = 0; i < n; i += 2) {
			/* Load the volume value */
			AE_L32X2_XC(volume, vol, inc);

			/* Load the input sample */
			AE_LA32X2_IP(in_sample, inu, in);

#if COMP_VOLUME_Q8_16
			/* Q8.16 x Q1.31 << 1 -> Q9.48 */
			mult0 = AE_MULF32S_HH(volume, in_sample);
			mult0 = AE_SRAI64(mult0, 1);			/* Q9.47 */
			mult1 = AE_MULF32S_LL(volume, in_sample);
			mult1 = AE_SRAI64(mult1, 1);
			out_sample = AE_ROUND32X2F48SSYM(mult0, mult1);	/* Q9.47 -> Q1.31 */
#elif COMP_VOLUME_Q1_23
			/* Q1.23 x Q1.31 << 1 -> Q2.55 */
			mult0 = AE_MULF32S_HH(volume, in_sample);
			mult0 = AE_SRAI64(mult0, 8);			/* Q2.47 */
			mult1 = AE_MULF32S_LL(volume, in_sample);
			mult1 = AE_SRAI64(mult1, 8);
			out_sample = AE_ROUND32X2F48SSYM(mult0, mult1);	/* Q2.47 -> Q1.31 */
#else
#error "Need CONFIG_COMP_VOLUME_Qx_y"
#endif
			AE_SA32X2_IP(out_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}

/**
 * \brief HiFi4 enabled volume passthrough from 32 bit to 24/32 or 32 bit.
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
	ae_f32x2 in_sample = AE_ZERO32();
	int i, n, m;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	const int channels_count = audio_stream_get_channels(sink);
	int samples = channels_count * frames;
	ae_f32x2 *in = (ae_f32x2 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						     + bsource->consumed);
	ae_f32x2 *out = (ae_f32x2 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						      + bsink->size);

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);
	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(m, n);
		inu = AE_LA64_PP(in);
		/* process two continuous sample data once */
		for (i = 0; i < n; i += 2) {
			/* Load the input sample */
			AE_LA32X2_IP(in_sample, inu, in);
			AE_SA32X2_IP(in_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
/**
 * \brief HiFi4 enabled volume processing from 16 bit to 16 bit.
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
	ae_f32x2 volume0 = AE_ZERO32();
	ae_f32x2 volume1 = AE_ZERO32();
	ae_f32x2 out_sample0 = AE_ZERO32();
	ae_f32x2 out_sample1 = AE_ZERO32();
	ae_f16x4 in_sample = AE_ZERO16();
	ae_f16x4 out_sample = AE_ZERO16();
	int i, n, m;
	ae_f32x2 *buf;
	ae_f32x2 *buf_end;
	ae_f32x2 *vol;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	ae_f16x4 *in = (ae_f16x4 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						     + bsource->consumed);
	ae_f16x4 *out = (ae_f16x4 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						      + bsink->size);
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32x2);
	int samples = channels_count * frames;

	/** to ensure the adsress is 8-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	if (cd->copy_gain)
		vol_store_gain(cd, channels_count);

	buf = (ae_f32x2 *)cd->vol;
	buf_end = (ae_f32x2 *)(cd->vol + channels_count * 4);
	vol = buf;

	/* Set buf as circular buffer */
	AE_SETCBEGIN0(buf);
	AE_SETCEND0(buf_end);

	while (samples) {
		m = audio_stream_samples_without_wrap_s16(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(m, n);
		inu = AE_LA64_PP(in);
		for (i = 0; i < n; i += 4) {
			/* load first two volume gain */
			AE_L32X2_XC(volume0, vol, inc);

			/* load second two volume gain */
			AE_L32X2_XC(volume1, vol, inc);

#if COMP_VOLUME_Q8_16
			/* Q8.16 to Q9.23 */
			volume0 = AE_SLAI32S(volume0, 7);
			volume1 = AE_SLAI32S(volume1, 7);
#elif COMP_VOLUME_Q1_23
			/* No need to shift, Q1.23 is OK as such */
#else
#error "Need CONFIG_COMP_VOLUME_Qx_y"
#endif
			/* Load the input sample */
			AE_LA16X4_IP(in_sample, inu, in);

			/* Multiply the input sample */
			out_sample0 = AE_MULFP32X16X2RS_H(volume0, in_sample);
			out_sample1 = AE_MULFP32X16X2RS_L(volume1, in_sample);

			/* Q9.23 to Q1.31 */
			out_sample0 = AE_SLAI32S(out_sample0, 8);
			out_sample1 = AE_SLAI32S(out_sample1, 8);

			/* store the output */
			out_sample = AE_ROUND16X4F32SSYM(out_sample0, out_sample1);
			// AE_SA16X4_IC(out_sample, outu, out);
			AE_SA16X4_IP(out_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		bsource->consumed += VOL_S16_SAMPLES_TO_BYTES(n);
		bsink->size += VOL_S16_SAMPLES_TO_BYTES(n);
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
}

/**
 * \brief HiFi4 enabled volume passthrough from 16 bit to 16 bit.
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
	ae_f16x4 in_sample = AE_ZERO16();
	int i, n, m;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	ae_f16x4 *in = (ae_f16x4 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						     + bsource->consumed);
	ae_f16x4 *out = (ae_f16x4 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
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
		inu = AE_LA64_PP(in);
		for (i = 0; i < n; i += 4) {
			/* Load the input sample */
			AE_LA16X4_IP(in_sample, inu, in);
			AE_SA16X4_IP(in_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
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

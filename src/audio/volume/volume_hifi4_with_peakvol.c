// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

/**
 * \file
 * \brief Volume HIFI4 processing implementation with peak volume detection
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

#if CONFIG_COMP_PEAK_VOL
#include <xtensa/tie/xt_hifi4.h>

static inline void vol_store_gain(struct vol_data *cd, const int channels_count)
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
 * \param[in] attenuation factor for peakmeter adjustment
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
	ae_f32x2 temp;
	ae_f32x2 *peakvol = (ae_f32x2 *)cd->peak_vol;

	/* Set peakvol(which stores the peak volume data twice) as circular buffer */
	AE_SETCBEGIN1(cd->peak_vol);
	AE_SETCEND1(cd->peak_vol  + channels_count * 2);

	/** to ensure the adsress is 8-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	if (cd->copy_gain)
		vol_store_gain(cd, channels_count);

	vol = (ae_f32x2 *)cd->vol;
	/* Set buf who stores the volume gain data as circular buffer */
	AE_SETCBEGIN0(vol);
	AE_SETCEND0(cd->vol + channels_count * 2);

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);

	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(m, n);
		inu = AE_LA64_PP(in);
		/* process two continuous sample data once */
		for (i = 0; i < n; i += 2) {
			/* Load the volume value */
			AE_L32X2_XC(volume, vol, inc);

			/* Load the input sample */
			AE_LA32X2_IP(in_sample, inu, in);
			/* calculate the peak volume*/
			AE_L32X2_XC1(temp, peakvol, 0);
			temp = AE_MAXABS32S(in_sample, temp);
			AE_S32X2_XC1(temp, peakvol, inc);
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
	for (i = 0; i < channels_count; i++)
		cd->peak_regs.peak_meter[i] = MAX(cd->peak_vol[i],
						  cd->peak_vol[i + channels_count])
						  << (attenuation + PEAK_24S_32C_ADJUST);
}

/**
 * \brief HiFi4 enabled volume passthrough from 24/32 bit to 24/32 or 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment
 */
static void vol_passthrough_s24_to_s24_s32(struct processing_module *mod,
					   struct input_stream_buffer *bsource,
					   struct output_stream_buffer *bsink, uint32_t frames,
					   uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
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
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32x2);
	int samples = channels_count * frames;
	ae_f32x2 temp;
	ae_f32x2 *peakvol = (ae_f32x2 *)cd->peak_vol;

	/* Set peakvol(which stores the peak volume data twice) as circular buffer */
	AE_SETCBEGIN1(cd->peak_vol);
	AE_SETCEND1(cd->peak_vol  + channels_count * 2);

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);

	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(m, n);
		inu = AE_LA64_PP(in);
		/* process two continuous sample data once */
		for (i = 0; i < n; i += 2) {
			/* Load the input sample */
			AE_LA32X2_IP(in_sample, inu, in);
			/* calculate the peak volume*/
			AE_L32X2_XC1(temp, peakvol, 0);
			temp = AE_MAXABS32S(in_sample, temp);
			AE_S32X2_XC1(temp, peakvol, inc);
			/* Store the output sample */
			AE_SA32X2_IP(in_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
	for (i = 0; i < channels_count; i++)
		cd->peak_regs.peak_meter[i] = MAX(cd->peak_vol[i],
						  cd->peak_vol[i + channels_count])
						  << (attenuation + PEAK_24S_32C_ADJUST);
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief HiFi4 enabled volume processing from 32 bit to 24/32 or 32 bit.
 * \param[in,out] mod Pointer to struct processing_module
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment
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
	ae_f32x2 temp;
	ae_f32x2 *peakvol = (ae_f32x2 *)cd->peak_vol;

	/* Set peakvol(which stores the peak volume data twice) as circular buffer */
	AE_SETCBEGIN1(cd->peak_vol);
	AE_SETCEND1(cd->peak_vol  + channels_count * 2);

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
			/* calculate the peak volume*/
			AE_L32X2_XC1(temp, peakvol, 0);
			temp = AE_MAXABS32S(in_sample, temp);
			AE_S32X2_XC1(temp, peakvol, inc);
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
	for (i = 0; i < channels_count; i++)
		cd->peak_regs.peak_meter[i] = MAX(cd->peak_vol[i],
						  cd->peak_vol[i + channels_count]) << attenuation;
}

/**
 * \brief HiFi4 enabled volume passthrough from 32 bit to 24/32 or 32 bit.
 * \param[in,out] mod Pointer to struct processing_module
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment
 */
static void vol_passthrough_s32_to_s24_s32(struct processing_module *mod,
					   struct input_stream_buffer *bsource,
					   struct output_stream_buffer *bsink, uint32_t frames,
					   uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	ae_f32x2 in_sample = AE_ZERO32();
	int i, n, m;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32x2);
	int samples = channels_count * frames;
	ae_f32x2 *in = (ae_f32x2 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						     + bsource->consumed);
	ae_f32x2 *out = (ae_f32x2 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						      + bsink->size);
	ae_f32x2 temp;
	ae_f32x2 *peakvol = (ae_f32x2 *)cd->peak_vol;

	/* Set peakvol(which stores the peak volume data twice) as circular buffer */
	AE_SETCBEGIN1(cd->peak_vol);
	AE_SETCEND1(cd->peak_vol  + channels_count * 2);
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
			/* calculate the peak volume*/
			AE_L32X2_XC1(temp, peakvol, 0);
			temp = AE_MAXABS32S(in_sample, temp);
			AE_S32X2_XC1(temp, peakvol, inc);

			AE_SA32X2_IP(in_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
	for (i = 0; i < channels_count; i++)
		cd->peak_regs.peak_meter[i] = MAX(cd->peak_vol[i],
						  cd->peak_vol[i + channels_count]) << attenuation;
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
/**
 * \brief HiFi4 enabled volume processing from 16 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused for 16bit)
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
	ae_f32x2 temp;
	ae_f32x2 *peakvol = (ae_f32x2 *)cd->peak_vol;

	/* Set peakvol(which stores the peak volume data 4 times) as circular buffer */
	AE_SETCBEGIN1(cd->peak_vol);
	AE_SETCEND1(cd->peak_vol  + channels_count * 4);

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
			/* calculate the peak volume*/
			AE_L32X2_XC1(temp, peakvol, 0);
			temp = AE_MAXABS32S(AE_SEXT32X2D16_32(in_sample), temp);
			AE_S32X2_XC1(temp, peakvol, inc);
			/* calculate the peak volume*/
			AE_L32X2_XC1(temp, peakvol, 0);
			temp = AE_MAXABS32S(AE_SEXT32X2D16_10(in_sample), temp);
			AE_S32X2_XC1(temp, peakvol, inc);
			/* Multiply the input sample */
			out_sample0 = AE_MULFP32X16X2RS_H(volume0, in_sample);
			out_sample1 = AE_MULFP32X16X2RS_L(volume1, in_sample);

			/* Q9.23 to Q1.31 */
			out_sample0 = AE_SLAI32S(out_sample0, 8);
			out_sample1 = AE_SLAI32S(out_sample1, 8);

			/* store the output */
			out_sample = AE_ROUND16X4F32SSYM(out_sample0, out_sample1);
			AE_SA16X4_IP(out_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
		bsource->consumed += VOL_S16_SAMPLES_TO_BYTES(n);
		bsink->size += VOL_S16_SAMPLES_TO_BYTES(n);
	}
	for (i = 0; i < channels_count; i++) {
		m = MAX(cd->peak_vol[i], cd->peak_vol[i + channels_count]);
		m = MAX(m, cd->peak_vol[i + channels_count * 2]);
		m = MAX(m, cd->peak_vol[i + channels_count * 3]);
		cd->peak_regs.peak_meter[i] = m << PEAK_16S_32C_ADJUST;
	}
}

/**
 * \brief HiFi4 enabled volume passthrough from 16 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused for 16bit)
 */
static void vol_passthrough_s16_to_s16(struct processing_module *mod,
				       struct input_stream_buffer *bsource,
				       struct output_stream_buffer *bsink, uint32_t frames,
				       uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
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
	const int inc = sizeof(ae_f32x2);
	int samples = channels_count * frames;
	ae_f32x2 temp;
	ae_f32x2 *peakvol = (ae_f32x2 *)cd->peak_vol;

	/* Set peakvol(which stores the peak volume data 4 times) as circular buffer */
	AE_SETCBEGIN1(cd->peak_vol);
	AE_SETCEND1(cd->peak_vol  + channels_count * 4);

	while (samples) {
		m = audio_stream_samples_without_wrap_s16(source, in);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(m, n);
		inu = AE_LA64_PP(in);
		for (i = 0; i < n; i += 4) {
			/* Load the input sample */
			AE_LA16X4_IP(in_sample, inu, in);
			/* calculate the peak volume*/
			AE_L32X2_XC1(temp, peakvol, 0);
			temp = AE_MAXABS32S(AE_SEXT32X2D16_32(in_sample), temp);
			AE_S32X2_XC1(temp, peakvol, inc);
			/* calculate the peak volume*/
			AE_L32X2_XC1(temp, peakvol, 0);
			temp = AE_MAXABS32S(AE_SEXT32X2D16_10(in_sample), temp);
			AE_S32X2_XC1(temp, peakvol, inc);
			/* store the output */
			AE_SA16X4_IP(in_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
		bsource->consumed += VOL_S16_SAMPLES_TO_BYTES(n);
		bsink->size += VOL_S16_SAMPLES_TO_BYTES(n);
	}
	for (i = 0; i < channels_count; i++) {
		m = MAX(cd->peak_vol[i], cd->peak_vol[i + channels_count]);
		m = MAX(m, cd->peak_vol[i + channels_count * 2]);
		m = MAX(m, cd->peak_vol[i + channels_count * 3]);
		cd->peak_regs.peak_meter[i] = m << PEAK_16S_32C_ADJUST;
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

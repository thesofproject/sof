// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

/**
 * \file
 * \brief Volume HiFi3 processing implementation with peak volume detection
 * \authors Andrula Song <andrula.song@intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(volume_hifi3, CONFIG_SOF_LOG_LEVEL);

#include "volume.h"

#ifdef VOLUME_HIFI3

#if CONFIG_COMP_PEAK_VOL

#include <xtensa/tie/xt_hifi3.h>

#if CONFIG_FORMAT_S24LE
/**
 * \brief HiFi3 enabled volume processing from 24/32 bit to 24/32 or 32 bit.
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
	int channel, n, i, m;
	ae_f32 *in0 = (ae_f32 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						  + bsource->consumed);
	ae_f32 *out0 = (ae_f32 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						   + bsink->size);
	ae_f32 *in, *out;
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32) * channels_count;
	int samples = channels_count * frames;
	ae_f32x2 peak_vol;
	uint32_t *peak_meter = cd->peak_regs.peak_meter;

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);
	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in0);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s32(sink, out0);
		n = MIN(m, n);
		for (channel = 0; channel < channels_count; channel++) {
			peak_vol = AE_ZERO32();
			/* set start address of sample load */
			in = in0 + channel;
			/* set start address of sample store */
			out = out0  + channel;
			/* Load volume */
			volume = (ae_f32x2)cd->volume[channel];
			for (i = 0; i < n; i += channels_count) {
				/* Load the input sample */
				AE_L32_XP(in_sample, in, inc);
				/* calc peak vol */
				peak_vol = AE_MAXABS32S(in_sample, peak_vol);
#if COMP_VOLUME_Q8_16
				/* Multiply the input sample */
				out_sample = AE_MULFP32X2RS(AE_SLAI32S(volume, 7),
							    AE_SLAI32(in_sample, 8));
#elif COMP_VOLUME_Q1_23
				out_sample = AE_MULFP32X2RS(volume, AE_SLAI32S(in_sample, 8));
#else
#error "Need CONFIG_COMP_VOLUME_Qx_y"
#endif
				/* Shift for S24_LE */
				out_sample = AE_SLAI32S(out_sample, 8);
				out_sample = AE_SRAI32(out_sample, 8);
				/* Store the output sample */
				AE_S32_L_XP(out_sample, out, inc);
			}
			peak_vol = AE_SLAA32S(peak_vol, attenuation + PEAK_24S_32C_ADJUST);
			peak_meter[channel] = AE_MAX32(peak_vol, peak_meter[channel]);
		}
		samples -= n;
		out0 = audio_stream_wrap(sink, out0 + n);
		in0 = audio_stream_wrap(source, in0 + n);
	}
}

/**
 * \brief HiFi3 enabled volume passthrough from 24/32 bit to 24/32 or 32 bit.
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
	int channel, n, i, m;
	ae_f32 *in0 = (ae_f32 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						  + bsource->consumed);
	ae_f32 *out0 = (ae_f32 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						   + bsink->size);
	ae_f32 *in, *out;
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32) * channels_count;
	int samples = channels_count * frames;
	ae_f32x2 peak_vol;
	uint32_t *peak_meter = cd->peak_regs.peak_meter;

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);
	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in0);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s32(sink, out0);
		n = MIN(m, n);
		for (channel = 0; channel < channels_count; channel++) {
			peak_vol = AE_ZERO32();
			/* set start address of sample load */
			in = in0 + channel;
			/* set start address of sample store */
			out = out0  + channel;
			for (i = 0; i < n; i += channels_count) {
				/* Load the input sample */
				AE_L32_XP(in_sample, in, inc);
				/* calc peak vol */
				peak_vol = AE_MAXABS32S(in_sample, peak_vol);
				/* Store the output sample */
				AE_S32_L_XP(in_sample, out, inc);
			}
			peak_vol = AE_SLAA32S(peak_vol, attenuation + PEAK_24S_32C_ADJUST);
			peak_meter[channel] = AE_MAX32(peak_vol, peak_meter[channel]);
		}
		samples -= n;
		out0 = audio_stream_wrap(sink, out0 + n);
		in0 = audio_stream_wrap(source, in0 + n);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief HiFi3 enabled volume processing from 32 bit to 24/32 or 32 bit.
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
	int i, n, channel, m;
	ae_f64 mult0;
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32) * channels_count;
	int samples = channels_count * frames;
	ae_f32 *in0 = (ae_f32 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						  + bsource->consumed);
	ae_f32 *out0 = (ae_f32 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						   + bsink->size);
	ae_f32 *in, *out;
	ae_f32x2 peak_vol;
	uint32_t *peak_meter = cd->peak_regs.peak_meter;

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);
	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in0);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s32(sink, out0);
		n = MIN(m, n);
		for (channel = 0; channel < channels_count; channel++) {
			peak_vol = AE_ZERO32();
			/* set start address of sample load */
			in = in0 + channel;
			/* set start address of sample store */
			out = out0  + channel;
			/* Load volume */
			volume = (ae_f32x2)cd->volume[channel];
			for (i = 0; i < n; i += channels_count) {
				/* Load the input sample */
				AE_L32_XP(in_sample, in, inc);
				/* calc peak vol */
				peak_vol = AE_MAXABS32S(in_sample, peak_vol);
#if COMP_VOLUME_Q8_16
				/* Q8.16 x Q1.31 << 1 -> Q9.48 */
				mult0 = AE_MULF32S_HH(volume, in_sample);
				mult0 = AE_SRAI64(mult0, 1);			/* Q9.47 */
				out_sample = AE_ROUND32F48SASYM(mult0);	/* Q9.47 -> Q1.31 */
#elif COMP_VOLUME_Q1_23
				/* Q1.23 x Q1.31 << 1 -> Q2.55 */
				mult0 = AE_MULF32S_HH(volume, in_sample);
				mult0 = AE_SRAI64(mult0, 8);			/* Q2.47 */
				out_sample = AE_ROUND32F48SSYM(mult0);	/* Q2.47 -> Q1.31 */
#else
#error "Need CONFIG_COMP_VOLUME_Qx_y"
#endif
				AE_S32_L_XP(out_sample, out, inc);
			}
			peak_vol = AE_SLAA32S(peak_vol, attenuation);
			peak_meter[channel] = AE_MAX32(peak_vol, peak_meter[channel]);
		}
		samples -= n;
		out0 = audio_stream_wrap(sink, out0 + n);
		in0 = audio_stream_wrap(source, in0 + n);
	}
}

/**
 * \brief HiFi3 enabled volume passthrough from 32 bit to 24/32 or 32 bit.
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
	int i, n, channel, m;
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f32) * channels_count;
	int samples = channels_count * frames;
	ae_f32 *in0 = (ae_f32 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						  + bsource->consumed);
	ae_f32 *out0 = (ae_f32 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						   + bsink->size);
	ae_f32 *in, *out;
	ae_f32x2 peak_vol;
	uint32_t *peak_meter = cd->peak_regs.peak_meter;

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(samples);
	while (samples) {
		m = audio_stream_samples_without_wrap_s32(source, in0);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s32(sink, out0);
		n = MIN(m, n);
		for (channel = 0; channel < channels_count; channel++) {
			peak_vol = AE_ZERO32();
			/* set start address of sample load */
			in = in0 + channel;
			/* set start address of sample store */
			out = out0  + channel;
			for (i = 0; i < n; i += channels_count) {
				/* Load the input sample */
				AE_L32_XP(in_sample, in, inc);
				/* calc peak vol */
				peak_vol = AE_MAXABS32S(in_sample, peak_vol);

				AE_S32_L_XP(in_sample, out, inc);
			}
			peak_vol = AE_SLAA32S(peak_vol, attenuation);
			peak_meter[channel] = AE_MAX32(peak_vol, peak_meter[channel]);
		}
		samples -= n;
		out0 = audio_stream_wrap(sink, out0 + n);
		in0 = audio_stream_wrap(source, in0 + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
/**
 * \brief HiFi3 enabled volume processing from 16 bit to 16 bit.
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
	ae_f32x2 volume = AE_ZERO32();
	ae_f32x2 out_sample0 = AE_ZERO32();
	ae_f16x4 in_sample = AE_ZERO16();
	ae_f16x4 out_sample = AE_ZERO16();
	int i, n, channel, m;
	ae_f16 *in;
	ae_f16 *out;
	ae_f16 *in0 = (ae_f16 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						  + bsource->consumed);
	ae_f16 *out0 = (ae_f16 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						   + bsink->size);
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f16) * channels_count;
	int samples = channels_count * frames;
	ae_f32x2 peak_vol;
	uint32_t *peak_meter = cd->peak_regs.peak_meter;

	while (samples) {
		m = audio_stream_samples_without_wrap_s16(source, in0);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s16(sink, out0);
		n = MIN(m, n);
		for (channel = 0; channel < channels_count; channel++) {
			peak_vol = AE_ZERO32();
			/* set start address of sample load */
			in = in0 + channel;
			/* set start address of sample store */
			out = out0  + channel;
			/* Load volume */
			volume = (ae_f32x2)cd->volume[channel];
#if COMP_VOLUME_Q8_16
			/* Q8.16 to Q9.23 */
			volume = AE_SLAI32S(volume, 7);
#elif COMP_VOLUME_Q1_23
			/* No need to shift, Q1.23 is OK as such */
#else
#error "Need CONFIG_COMP_VOLUME_Qx_y"
#endif
			for (i = 0; i < n; i += channels_count) {
				/* Load the input sample */
				AE_L16_XP(in_sample, in, inc);

				/* calc peak vol */
				peak_vol = AE_MAXABS32S(AE_SEXT32X2D16_32(in_sample), peak_vol);
				/* Multiply the input sample */
				out_sample0 = AE_MULFP32X16X2RS_H(volume, in_sample);

				/* Q9.23 to Q1.31 */
				out_sample0 = AE_SLAI32S(out_sample0, 8);

				/* store the output */
				out_sample = AE_ROUND16X4F32SSYM(out_sample0, out_sample0);
				// AE_SA16X4_IC(out_sample, outu, out);
				AE_S16_0_XP(out_sample, out, inc);
			}
			peak_vol = AE_SLAA32(peak_vol, PEAK_16S_32C_ADJUST);
			peak_meter[channel] = AE_MAX32(peak_vol, peak_meter[channel]);
		}
		out0 = audio_stream_wrap(sink, out0 + n);
		in0 = audio_stream_wrap(source, in0 + n);
		samples -= n;
		bsource->consumed += VOL_S16_SAMPLES_TO_BYTES(n);
		bsink->size += VOL_S16_SAMPLES_TO_BYTES(n);
	}
}

/**
 * \brief HiFi3 enabled volume passthrough from 16 bit to 16 bit.
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
	int i, n, channel, m;
	ae_f16 *in;
	ae_f16 *out;
	ae_f16 *in0 = (ae_f16 *)audio_stream_wrap(source, (char *)audio_stream_get_rptr(source)
						  + bsource->consumed);
	ae_f16 *out0 = (ae_f16 *)audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink)
						   + bsink->size);
	const int channels_count = audio_stream_get_channels(sink);
	const int inc = sizeof(ae_f16) * channels_count;
	int samples = channels_count * frames;
	ae_f32x2 peak_vol;
	uint32_t *peak_meter = cd->peak_regs.peak_meter;

	while (samples) {
		m = audio_stream_samples_without_wrap_s16(source, in0);
		n = MIN(m, samples);
		m = audio_stream_samples_without_wrap_s16(sink, out0);
		n = MIN(m, n);
		for (channel = 0; channel < channels_count; channel++) {
			peak_vol = AE_ZERO32();
			/* set start address of sample load */
			in = in0 + channel;
			/* set start address of sample store */
			out = out0  + channel;
			for (i = 0; i < n; i += channels_count) {
				/* Load the input sample */
				AE_L16_XP(in_sample, in, inc);

				/* calc peak vol */
				peak_vol = AE_MAXABS32S(AE_SEXT32X2D16_32(in_sample), peak_vol);

				/* store the output */
				AE_S16_0_XP(in_sample, out, inc);
			}
			peak_vol = AE_SLAA32(peak_vol, PEAK_16S_32C_ADJUST);
			peak_meter[channel] = AE_MAX32(peak_vol, peak_meter[channel]);
		}
		out0 = audio_stream_wrap(sink, out0 + n);
		in0 = audio_stream_wrap(source, in0 + n);
		samples -= n;
		bsource->consumed += VOL_S16_SAMPLES_TO_BYTES(n);
		bsink->size += VOL_S16_SAMPLES_TO_BYTES(n);
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

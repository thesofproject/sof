// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

/**
 * \file
 * \brief Volume generic processing implementation with peak volume detection
 * \authors Andrula Song <andrula.song@intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_DECLARE(volume_generic, CONFIG_SOF_LOG_LEVEL);

#include <sof/audio/volume.h>

#ifdef CONFIG_GENERIC

#if CONFIG_COMP_PEAK_VOL

#if CONFIG_FORMAT_S24LE
/**
 * \brief Volume s24 to s24 multiply function
 * \param[in] x   input sample.
 * \param[in] vol gain.
 * \return output sample.
 *
 * Volume multiply for 24 bit input and 24 bit output.
 */
static inline int32_t vol_mult_s24_to_s24(int32_t x, int32_t vol)
{
	return q_multsr_sat_32x32_24(sign_extend_s24(x), vol, Q_SHIFT_BITS_64(23, VOL_QXY_Y, 23));
}

/**
 * \brief Volume processing from 24/32 bit to 24/32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 24/32 bit destination buffer.
 */
static void vol_s24_to_s24(struct processing_module *mod, struct input_stream_buffer *bsource,
			   struct output_stream_buffer *bsink, uint32_t frames,
			   uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	int32_t vol;
	int32_t *x, *x0;
	int32_t *y, *y0;
	int nmax, n, i, j;
	const int nch = source->channels;
	int remaining_samples = frames * nch;
	int32_t tmp;

	memset(cd->peak_regs.peak_meter, 0, sizeof(uint32_t) * cd->channels);
	x = audio_stream_wrap(source, (char *)source->r_ptr + bsource->consumed);
	y = audio_stream_wrap(sink, (char *)sink->w_ptr + bsink->size);

	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(remaining_samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(remaining_samples);
	while (remaining_samples) {
		nmax = audio_stream_samples_without_wrap_s24(source, x);
		n = MIN(remaining_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s24(sink, y);
		n = MIN(n, nmax);
		for (j = 0; j < nch; j++) {
			x0 = x + j;
			y0 = y + j;
			vol = cd->volume[j];
			tmp = 0;
			for (i = 0; i < n; i += nch) {
				y0[i] = vol_mult_s24_to_s24(x0[i], vol);
				tmp = MAX(abs(x0[i]), tmp);
			}
			tmp = tmp << attenuation;
			cd->peak_regs.peak_meter[j] = MAX(tmp, cd->peak_regs.peak_meter[j]);
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
	/* update peak vol */
	peak_vol_update(cd);
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief Volume processing from 32 bit to 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment
 *
 * Copy and scale volume from 32 bit source buffer
 * to 32 bit destination buffer.
 */
static void vol_s32_to_s32(struct processing_module *mod, struct input_stream_buffer *bsource,
			   struct output_stream_buffer *bsink, uint32_t frames,
			   uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	int32_t vol;
	int32_t *x, *x0;
	int32_t *y, *y0;
	int nmax, n, i, j;
	const int nch = source->channels;
	int remaining_samples = frames * nch;
	int32_t tmp;

	memset(cd->peak_regs.peak_meter, 0, sizeof(uint32_t) * cd->channels);
	x = audio_stream_wrap(source, (char *)source->r_ptr + bsource->consumed);
	y = audio_stream_wrap(sink, (char *)sink->w_ptr + bsink->size);
	bsource->consumed += VOL_S32_SAMPLES_TO_BYTES(remaining_samples);
	bsink->size += VOL_S32_SAMPLES_TO_BYTES(remaining_samples);
	while (remaining_samples) {
		nmax = audio_stream_samples_without_wrap_s32(source, x);
		n = MIN(remaining_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, y);
		n = MIN(n, nmax);
		/* Note: on Xtensa processing one channel volume at time performed slightly
		 * better than simpler interleaved code version (average 19 us vs. 20 us).
		 */
		for (j = 0; j < nch; j++) {
			x0 = x + j;
			y0 = y + j;
			vol = cd->volume[j];
			tmp = 0;
			for (i = 0; i < n; i += nch) {
				y0[i] = q_multsr_sat_32x32(x0[i], vol,
							   Q_SHIFT_BITS_64(31, VOL_QXY_Y, 31));
				tmp = MAX(abs(x0[i]), tmp);
			}
			tmp = tmp << attenuation;
			cd->peak_regs.peak_meter[j] = MAX(tmp, cd->peak_regs.peak_meter[j]);
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}

	/* update peak vol */
	peak_vol_update(cd);
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
/**
 * \brief Volume processing from 16 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Input buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] attenuation factor for peakmeter adjustment (unused for 16bit)
 *
 * Copy and scale volume from 16 bit source buffer
 * to 16 bit destination buffer.
 */
static void vol_s16_to_s16(struct processing_module *mod, struct input_stream_buffer *bsource,
			   struct output_stream_buffer *bsink, uint32_t frames,
			   uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	int32_t vol;
	int16_t *x, *x0;
	int16_t *y, *y0;
	int nmax, n, i, j;
	const int nch = source->channels;
	int remaining_samples = frames * nch;
	int32_t tmp;

	memset(cd->peak_regs.peak_meter, 0, sizeof(uint32_t) * cd->channels);
	x = audio_stream_wrap(source, (char *)source->r_ptr + bsource->consumed);
	y = audio_stream_wrap(sink, (char *)sink->w_ptr + bsink->size);

	bsource->consumed += VOL_S16_SAMPLES_TO_BYTES(remaining_samples);
	bsink->size += VOL_S16_SAMPLES_TO_BYTES(remaining_samples);
	while (remaining_samples) {
		nmax = audio_stream_samples_without_wrap_s16(source, x);
		n = MIN(remaining_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s16(sink, y);
		n = MIN(n, nmax);
		for (j = 0; j < nch; j++) {
			x0 = x + j;
			y0 = y + j;
			vol = cd->volume[j];
			tmp = 0;
			for (i = 0; i < n; i += nch) {
				y0[i] = q_multsr_sat_32x32_16(x0[i], vol,
							      Q_SHIFT_BITS_32(15, VOL_QXY_Y, 15));
				tmp = MAX(abs(x0[i]), tmp);
			}
			cd->peak_regs.peak_meter[j] = MAX(tmp, cd->peak_regs.peak_meter[j]);
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}

	/* update peak vol */
	peak_vol_update(cd);
}
#endif

const struct comp_func_map volume_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, vol_s16_to_s16 },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, vol_s24_to_s24 },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, vol_s32_to_s32 },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t volume_func_count = ARRAY_SIZE(volume_func_map);
#endif
#endif

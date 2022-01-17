// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file
 * \brief Volume HiFi3 processing implementation
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/audio/volume.h>

#if defined(__XCC__) && XCHAL_HAVE_HIFI3

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <xtensa/tie/xt_hifi3.h>
#include <stddef.h>
#include <stdint.h>

/**
 * \brief Sets buffer to be circular using HiFi3 functions.
 * \param[in,out] buffer Circular buffer.
 */
static void vol_setup_circular(const struct audio_stream *buffer)
{
	AE_SETCBEGIN0(buffer->addr);
	AE_SETCEND0(buffer->end_addr);
}

#if CONFIG_FORMAT_S24LE
/**
 * \brief HiFi3 enabled volume processing from 24/32 bit to 24/32 or 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 */
static void vol_s24_to_s24_s32(struct comp_dev *dev, struct audio_stream *sink,
			       const struct audio_stream *source,
			       uint32_t frames)
{
	struct vol_data *cd = comp_get_drvdata(dev);
	ae_f64 mult;
	ae_f32x2 in_sample = AE_ZERO32();
	ae_f32x2 out_sample;
	ae_f32x2 volume;
	size_t channel;
	int i;
	int shift = 8;
	const int inc = sizeof(ae_int32) * sink->channels;

	/* Processing per channel */
	for (channel = 0; channel < sink->channels; channel++) {
		/* Load volume */
		volume = (ae_f32x2)cd->volume[channel];

		/* setting start address of sample load */
		ae_int32 *in = (ae_int32 *)source->r_ptr + channel;

		/* setting start address of sample store */
		ae_int32 *out = (ae_int32 *)sink->w_ptr + channel;
		/* Main processing loop */
		for (i = 0; i < frames; i++) {
			/* Set source as circular buffer */
			vol_setup_circular(source);

			/* Load the input sample */
			AE_L32_XC(in_sample, in, inc);

			/* Multiply the input sample */
			mult = AE_MULF32S_LL(volume, AE_SLAA32(in_sample, 8));

			/* Multiplication of Q1.31 x Q1.31 gives Q1.63.
			 * Now multiplication is Q8.16 x Q1.31, the result
			 * is Q9.48. Need to shift right by one to get Q17.47
			 * compatible format for round.
			 */
			out_sample = AE_ROUND32F48SSYM(AE_SRAI64(mult, 1));

			/* Shift for S24_LE */
			out_sample = AE_SRAA32RS(out_sample, shift);
			out_sample = AE_SLAA32S(out_sample, shift);
			out_sample = AE_SRAA32(out_sample, shift);

			/* Set sink as circular buffer */
			vol_setup_circular(sink);

			/* Store the output sample */
			AE_S32_L_XC(out_sample, out, inc);
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief HiFi3 enabled volume processing from 32 bit to 24/32 or 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 */
static void vol_s32_to_s24_s32(struct comp_dev *dev, struct audio_stream *sink,
			       const struct audio_stream *source,
			       uint32_t frames)
{
	struct vol_data *cd = comp_get_drvdata(dev);
	ae_f64 mult;
	ae_f32x2 in_sample = AE_ZERO32();
	ae_f32x2 out_sample;
	ae_f32x2 volume;
	size_t channel;
	int shift = 0;
	int i;
	const int inc = sizeof(ae_int32) * sink->channels;

	/* Processing per channel */
	for (channel = 0; channel < sink->channels; channel++) {
		ae_int32 *in = (ae_int32 *)source->r_ptr + channel;
		ae_int32 *out = (ae_int32 *)sink->w_ptr + channel;
		/* Load volume */
		volume = (ae_f32x2)cd->volume[channel];
		/* Main processing loop */
		for (i = 0; i < frames; i++) {
			/* Set source as circular buffer */
			vol_setup_circular(source);

			/* Load the input sample */
			AE_L32_XC(in_sample, in, inc);

			/* Multiply the input sample */
			mult = AE_MULF32S_LL(volume, in_sample);

			/* Multiplication of Q1.31 x Q1.31 gives Q1.63.
			 * Now multiplication is Q8.16 x Q1.31, the result
			 * is Q9.48. Need to shift right by one to get Q17.47
			 * compatible format for round.
			 */
			out_sample = AE_ROUND32F48SSYM(AE_SRAI64(mult, 1));

			/* Shift for S24_LE */
			out_sample = AE_SRAA32RS(out_sample, shift);
			out_sample = AE_SLAA32S(out_sample, shift);
			out_sample = AE_SRAA32(out_sample, shift);

			/* Set sink as circular buffer */
			vol_setup_circular(sink);

			/* Store the output sample */
			AE_S32_L_XC(out_sample, out, inc);
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
/**
 * \brief HiFi3 enabled volume processing from 16 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 */
static void vol_s16_to_s16(struct comp_dev *dev, struct audio_stream *sink,
			   const struct audio_stream *source, uint32_t frames)
{
	struct vol_data *cd = comp_get_drvdata(dev);
	ae_f64 mult;
	ae_f32x2 volume;
	ae_f32x2 out_sample;
	ae_f16x4 in_sample = AE_ZERO16();
	size_t channel;
	int i;
	const int inc = sizeof(ae_int16) * sink->channels;

	/* Processing per channel */
	for (channel = 0; channel < sink->channels; channel++) {
		ae_int16 *in = (ae_int16 *)source->r_ptr + channel;
		ae_int16 *out = (ae_int16 *)sink->w_ptr + channel;
		/* Load volume */
		volume = (ae_f32x2)cd->volume[channel];
		for (i = 0; i < frames; i++) {
			/* Set source as circular buffer */
			vol_setup_circular(source);

			/* Load the input sample */
			AE_L16_XC(in_sample, in, inc);

			/* Multiply the input sample */
			mult = AE_MULF32X16_L0(volume, in_sample);

			/* Multiply of Q1.31 x Q1.15 gives Q1.47. Multiply of
			 * Q8.16 x Q1.15 gives Q8.32, so need to shift left
			 * by 31 to get Q1.63. Sample is Q1.31.
			 */
			out_sample = AE_ROUND32F64SSYM(AE_SLAI64S(mult, 31));

			/* Set sink as circular buffer */
			vol_setup_circular(sink);

			/* Round to Q1.15 and store the output sample */
			AE_S16_0_XC(AE_ROUND16X4F32SSYM(out_sample, out_sample),
						out, inc);
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

const struct comp_func_map func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, vol_s16_to_s16 },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, vol_s24_to_s24_s32 },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, vol_s32_to_s24_s32 },
#endif
};

const size_t func_count = ARRAY_SIZE(func_map);

#endif

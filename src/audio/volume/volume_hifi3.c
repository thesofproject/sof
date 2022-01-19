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
	ae_f32x2 in_sample = AE_ZERO32();
	ae_f32x2 out_sample;
	ae_f32x2 volume;
	ae_f32x2 *buf;
	ae_f32x2 *buf_end;
	ae_valign inu;
	ae_valign outu;
	int i;
	ae_f32x2 *in = (ae_f32x2 *)source->r_ptr;
	ae_f32x2 *out = (ae_f32x2 *)sink->w_ptr;
	ae_f32x2 *vol;
	const int channels_count = sink->channels;
	const int inc = sizeof(ae_f32x2);
	const int samples = channels_count * frames;

	/** to ensure the adsress is 8-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	vol_store_gain(cd, channels_count);
	buf = (ae_f32x2 *)cd->vol;
	buf_end = (ae_f32x2 *)(cd->vol + channels_count * 2);
	vol = (ae_f32x2 *)buf;

	/* use alignment register to prime the memory to
	 * avoid risk of buf not aligned to 64 bits.
	 */
	AE_LA32X2POS_PC(inu, in);
	AE_SA64POS_FC(outu, out);

	/* process two continuous sample data once */
	for (i = 0; i < samples; i += 2) {
		/* Set buf who stores the volume gain data as circular buffer */
		AE_SETCBEGIN0(buf);
		AE_SETCEND0(buf_end);

		/* Load the volume value */
		AE_L32X2_XC(volume, vol, inc);

		/* Set source as circular buffer */
		vol_setup_circular(source);

		/* Load the input sample */
		AE_LA32X2_IC(in_sample, inu, in);

		/* Multiply the input sample */
		out_sample = AE_MULFP32X2RS(AE_SLAA32S(volume, 7), AE_SLAA32(in_sample, 8));

		/* Shift for S24_LE */
		out_sample = AE_SLAA32S(out_sample, 8);
		out_sample = AE_SRAA32(out_sample, 8);

		/* Set sink as circular buffer */
		vol_setup_circular(sink);

		/* Store the output sample */
		AE_SA32X2_IC(out_sample, outu, out);

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
	ae_f32x2 in_sample = AE_ZERO32();
	ae_f32x2 out_sample;
	ae_f32x2 volume;
	int i;
	ae_f64 mult0;
	ae_f64 mult1;
	ae_f32x2 *buf;
	ae_f32x2 *buf_end;
	ae_f32x2 *vol;
	const int inc = sizeof(ae_f32x2);
	const int channels_count = sink->channels;
	const int samples = channels_count * frames;
	ae_f32x2 *in = (ae_f32x2 *)source->r_ptr;
	ae_f32x2 *out = (ae_f32x2 *)sink->w_ptr;
	ae_valign inu;
	ae_valign outu;

	/** to ensure the address is 8-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	vol_store_gain(cd, channels_count);
	buf = (ae_f32x2 *)cd->vol;
	buf_end = (ae_f32x2 *)(cd->vol + channels_count * 2);
	vol = (ae_f32x2 *)buf;

	/* use alignment register to prime the memory to
	 * avoid risk of buf not aligned to 64 bits.
	 */
	AE_LA32X2POS_PC(inu, in);
	AE_SA64POS_FC(outu, out);

	/* process two continuous sample data once */
	for (i = 0; i < samples; i += 2) {
		/* Set buf who stores the volume gain data as circular buffer */
		AE_SETCBEGIN0(buf);
		AE_SETCEND0(buf_end);

		/* Load the volume value */
		AE_L32X2_XC(volume, vol, inc);

		/* Set source as circular buffer */
		vol_setup_circular(source);

		/* Load the input sample */
		AE_LA32X2_IC(in_sample, inu, in);

		mult0 = AE_MULF32S_HH(volume, in_sample);
		mult0 = AE_SRAI64(mult0, 1);
		mult1 = AE_MULF32S_LL(volume, in_sample);
		mult1 = AE_SRAI64(mult1, 1);
		out_sample = AE_ROUND32X2F48SSYM(mult0, mult1);

		vol_setup_circular(sink);
		AE_SA32X2_IC(out_sample, outu, out);
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
	ae_f32x2 volume0, volume1;
	ae_f32x2 out_sample0, out_sample1;
	ae_f16x4 in_sample = AE_ZERO16();
	ae_f16x4 out_sample = AE_ZERO16();
	int i;
	ae_f32x2 *buf;
	ae_f32x2 *buf_end;
	ae_f32x2 *vol;
	ae_valign inu;
	ae_valign outu;
	ae_f16x4 *in = (ae_f16x4 *)source->r_ptr;
	ae_f16x4 *out = (ae_f16x4 *)sink->w_ptr;
	const int channels_count = sink->channels;
	const int inc = sizeof(ae_f32x2);
	const int samples = channels_count * frames;

	/** to ensure the adsress is 8-byte aligned and avoid risk of
	 * error loading of volume gain while the cd->vol would be set
	 * as circular buffer
	 */
	vol_store_gain(cd, channels_count);
	buf = (ae_f32x2 *)cd->vol;
	buf_end = (ae_f32x2 *)(cd->vol + channels_count * 4);
	vol = buf;

	/*
	 * use alignment register to prime the volume memory to avoid
	 * risk of buf not aligned to 8-byte
	 */
	AE_LA16X4POS_PC(inu, in);
	AE_SA64POS_FC(outu, out);

	for (i = 0; i < samples; i += 4) {
		/* Set buf as circular buffer */
		AE_SETCBEGIN0(buf);
		AE_SETCEND0(buf_end);

		/* load first two volume gain */
		AE_L32X2_XC(volume0, vol, inc);

		/* load second two volume gain */
		AE_L32X2_XC(volume1, vol, inc);

		/* Q8.16 to Q9.23 */
		volume0 = AE_SLAA32(volume0, 7);
		volume1 = AE_SLAA32(volume1, 7);
		/* Set source as circular buffer */
		vol_setup_circular(source);

		/* Load the input sample */
		AE_LA16X4_IC(in_sample, inu, in);

		/* Multiply the input sample */
		out_sample0 = AE_MULFP32X16X2RS_H(volume0, in_sample);
		out_sample1 = AE_MULFP32X16X2RS_L(volume1, in_sample);

		/* Q9.23 to Q1.31 */
		out_sample0 = AE_SLAA32S(out_sample0, 8);
		out_sample1 = AE_SLAA32S(out_sample1, 8);

		/* Set sink as circular buffer */
		vol_setup_circular(sink);

		/* store the output */
		out_sample = AE_ROUND16X4F32SSYM(out_sample0, out_sample1);
		AE_SA16X4_IC(out_sample, outu, out);

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

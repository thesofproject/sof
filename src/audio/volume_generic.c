/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file audio/volume_generic.c
 * \brief Volume generic processing implementation
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>\n
 *          Keyon Jie <yang.jie@linux.intel.com>\n
 *          Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include "volume.h"

#ifdef CONFIG_GENERIC

/**
 * \brief Volume processing from 16 bit to 32 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 32 bit destination buffer for 2 channels.
 */
static void vol_s16_to_s32_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = (int32_t)src[i] * cd->volume[0];
		dest[i + 1] = (int32_t)src[i + 1] * cd->volume[1];
	}
}

/**
 * \brief Volume processing from 32 bit to 16 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 16 bit destination buffer for 2 channels.
 */
static void vol_s32_to_s16_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = (int16_t)q_multsr_sat_32x32(
			src[i], cd->volume[0], Q_SHIFT_BITS_64(31, 16, 15));
		dest[i + 1] = (int16_t)q_multsr_sat_32x32(
			src[i + 1], cd->volume[1], Q_SHIFT_BITS_64(31, 16, 15));
	}
}

/**
 * \brief Volume processing from 32 bit to 32 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 32 bit destination buffer for 2 channels.
 */
static void vol_s32_to_s32_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = q_multsr_sat_32x32(
			src[i], cd->volume[0], Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 1] = q_multsr_sat_32x32(
			src[i + 1], cd->volume[1], Q_SHIFT_BITS_64(31, 16, 31));
	}
}

/**
 * \brief Volume processing from 16 bit to 16 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 16 bit destination buffer for 2 channels.
 */
static void vol_s16_to_s16_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = q_multsr_sat_16x16(
			src[i], cd->volume[0], Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 1] = q_multsr_sat_16x16(
			src[i + 1], cd->volume[1], Q_SHIFT_BITS_32(15, 16, 15));
	}
}

/**
 * \brief Volume processing from 16 bit to 24/32 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 24/32 bit destination buffer for 2 channels.
 */
static void vol_s16_to_s24_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = q_multsr_sat_32x32(
			src[i], cd->volume[0], Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(
			src[i + 1], cd->volume[1], Q_SHIFT_BITS_64(15, 16, 23));
	}
}

/**
 * \brief Volume processing from 24/32 bit to 16 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 16 bit destination buffer for 2 channels.
 */
static void vol_s24_to_s16_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = (int16_t)q_multsr_sat_32x32(
			sign_extend_s24(src[i]), cd->volume[0],
			Q_SHIFT_BITS_64(23, 16, 15));
		dest[i + 1] = (int16_t)q_multsr_sat_32x32(
			sign_extend_s24(src[i + 1]), cd->volume[1],
			Q_SHIFT_BITS_64(23, 16, 15));
	}
}

/**
 * \brief Volume processing from 32 bit to 24/32 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 24/32 bit destination buffer for 2 channels.
 */
static void vol_s32_to_s24_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.23 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = q_multsr_sat_32x32(
			src[i], cd->volume[0], Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(
			src[i + 1], cd->volume[1], Q_SHIFT_BITS_64(31, 16, 23));
	}
}

/**
 * \brief Volume processing from 24/32 bit to 32 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 32 bit destination buffer for 2 channels.
 */
static void vol_s24_to_s32_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = q_multsr_sat_32x32(
			sign_extend_s24(src[i]), cd->volume[0],
			Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 1] = q_multsr_sat_32x32(
			sign_extend_s24(src[i + 1]), cd->volume[1],
			Q_SHIFT_BITS_64(23, 16, 31));
	}
}

/**
 * \brief Volume processing from 24/32 bit to 24/32 bit in 2 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 24/32 bit destination buffer for 2 channels.
 */
static void vol_s24_to_s24_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t i, *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.23 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 2; i += 2) {
		dest[i] = q_multsr_sat_32x32(
			sign_extend_s24(src[i]), cd->volume[0],
			Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(
			sign_extend_s24(src[i + 1]), cd->volume[1],
			Q_SHIFT_BITS_64(23, 16, 23));
	}
}

#if PLATFORM_MAX_CHANNELS >= 4
/**
 * \brief Volume processing from 16 bit to 32 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 32 bit destination buffer for 4 channels.
 */
static void vol_s16_to_s32_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		dest[i] = (int32_t)src[i] * cd->volume[0];
		dest[i + 1] = (int32_t)src[i + 1] * cd->volume[1];
		dest[i + 2] = (int32_t)src[i + 2] * cd->volume[2];
		dest[i + 3] = (int32_t)src[i + 3] * cd->volume[3];
	}
}

/**
 * \brief Volume processing from 32 bit to 16 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 16 bit destination buffer for 4 channels.
 */
static void vol_s32_to_s16_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		dest[i] = (int16_t)q_multsr_sat_32x32(src[i], cd->volume[0],
						      Q_SHIFT_BITS_64(31, 16,
								      15));
		dest[i + 1] = (int16_t)q_multsr_sat_32x32(src[i + 1],
							  cd->volume[1],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
		dest[i + 2] = (int16_t)q_multsr_sat_32x32(src[i + 2],
							  cd->volume[2],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
		dest[i + 3] = (int16_t)q_multsr_sat_32x32(src[i + 3],
							  cd->volume[3],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
	}
}

/**
 * \brief Volume processing from 32 bit to 32 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 32 bit destination buffer for 4 channels.
 */
static void vol_s32_to_s32_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		dest[i] = q_multsr_sat_32x32(src[i], cd->volume[0],
					     Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 1] = q_multsr_sat_32x32(src[i + 1], cd->volume[1],
						 Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 2] = q_multsr_sat_32x32(src[i + 2], cd->volume[2],
						 Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 3] = q_multsr_sat_32x32(src[i + 3], cd->volume[3],
						 Q_SHIFT_BITS_64(31, 16, 31));
	}
}

/**
 * \brief Volume processing from 16 bit to 16 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 16 bit destination buffer for 4 channels.
 */
static void vol_s16_to_s16_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		dest[i] = q_multsr_sat_16x16(src[i], cd->volume[0],
					     Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 1] = q_multsr_sat_16x16(src[i + 1], cd->volume[1],
						 Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 2] = q_multsr_sat_16x16(src[i + 2], cd->volume[2],
						 Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 3] = q_multsr_sat_16x16(src[i + 3], cd->volume[3],
						 Q_SHIFT_BITS_32(15, 16, 15));
	}
}

/**
 * \brief Volume processing from 16 bit to 24/32 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 24/32 bit destination buffer for 4 channels.
 */
static void vol_s16_to_s24_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		dest[i] = q_multsr_sat_32x32(src[i], cd->volume[0],
					     Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(src[i + 1], cd->volume[1],
						 Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 2] = q_multsr_sat_32x32(src[i + 2], cd->volume[2],
						 Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 3] = q_multsr_sat_32x32(src[i + 3], cd->volume[3],
						 Q_SHIFT_BITS_64(15, 16, 23));
	}
}

/**
 * \brief Volume processing from 24/32 bit to 16 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 16 bit destination buffer for 4 channels.
 */
static void vol_s24_to_s16_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i, sample;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		sample = sign_extend_s24(src[i]);
		dest[i] = (int16_t)q_multsr_sat_32x32(sample, cd->volume[0],
						      Q_SHIFT_BITS_64(23, 16,
								      15));
		sample = sign_extend_s24(src[i + 1]);
		dest[i + 1] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[1],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
		sample = sign_extend_s24(src[i + 2]);
		dest[i + 2] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[2],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
		sample = sign_extend_s24(src[i + 3]);
		dest[i + 3] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[3],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
	}
}

/**
 * \brief Volume processing from 32 bit to 24/32 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 24/32 bit destination buffer for 4 channels.
 */
static void vol_s32_to_s24_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.23 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		dest[i] = q_multsr_sat_32x32(src[i], cd->volume[0],
					     Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(src[i + 1], cd->volume[1],
						 Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 2] = q_multsr_sat_32x32(src[i + 2], cd->volume[2],
						 Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 3] = q_multsr_sat_32x32(src[i + 3], cd->volume[3],
						 Q_SHIFT_BITS_64(31, 16, 23));
	}
}

/**
 * \brief Volume processing from 24/32 bit to 32 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 32 bit destination buffer for 4 channels.
 */
static void vol_s24_to_s32_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		dest[i] = q_multsr_sat_32x32(sign_extend_s24(src[i]),
					     cd->volume[0],
					     Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 1] = q_multsr_sat_32x32(sign_extend_s24(src[i + 1]),
						 cd->volume[1],
						 Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 2] = q_multsr_sat_32x32(sign_extend_s24(src[i + 2]),
						 cd->volume[2],
						 Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 3] = q_multsr_sat_32x32(sign_extend_s24(src[i + 3]),
						 cd->volume[3],
						 Q_SHIFT_BITS_64(23, 16, 31));
	}
}

/**
 * \brief Volume processing from 24/32 bit to 24/32 bit in 4 channels.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 24/32 bit destination buffer for 4 channels.
 */
static void vol_s24_to_s24_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t i, *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.23 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 4; i += 4) {
		dest[i] = q_multsr_sat_32x32(sign_extend_s24(src[i]),
					     cd->volume[0],
					     Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(sign_extend_s24(src[i + 1]),
						 cd->volume[1],
						 Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 2] = q_multsr_sat_32x32(sign_extend_s24(src[i + 2]),
						 cd->volume[2],
						 Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 3] = q_multsr_sat_32x32(sign_extend_s24(src[i + 3]),
						 cd->volume[3],
						 Q_SHIFT_BITS_64(23, 16, 23));
	}
}
#endif

#if PLATFORM_MAX_CHANNELS >= 8
/* volume scaling functions for 8-channel input */

/* copy and scale volume from 16 bit source buffer to 32 bit dest buffer */
static void vol_s16_to_s32_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		dest[i] = (int32_t)src[i] * cd->volume[0];
		dest[i + 1] = (int32_t)src[i + 1] * cd->volume[1];
		dest[i + 2] = (int32_t)src[i + 2] * cd->volume[2];
		dest[i + 3] = (int32_t)src[i + 3] * cd->volume[3];
		dest[i + 4] = (int32_t)src[i + 4] * cd->volume[4];
		dest[i + 5] = (int32_t)src[i + 5] * cd->volume[5];
		dest[i + 6] = (int32_t)src[i + 6] * cd->volume[6];
		dest[i + 7] = (int32_t)src[i + 7] * cd->volume[7];
	}
}

/* copy and scale volume from 32 bit source buffer to 16 bit dest buffer */
static void vol_s32_to_s16_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		dest[i] = (int16_t)q_multsr_sat_32x32(src[i], cd->volume[0],
						      Q_SHIFT_BITS_64(31, 16,
								      15));
		dest[i + 1] = (int16_t)q_multsr_sat_32x32(src[i + 1],
							  cd->volume[1],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
		dest[i + 2] = (int16_t)q_multsr_sat_32x32(src[i + 2],
							  cd->volume[2],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
		dest[i + 3] = (int16_t)q_multsr_sat_32x32(src[i + 3],
							  cd->volume[3],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
		dest[i + 4] = (int16_t)q_multsr_sat_32x32(src[i + 4],
							  cd->volume[4],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
		dest[i + 5] = (int16_t)q_multsr_sat_32x32(src[i + 5],
							  cd->volume[5],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
		dest[i + 6] = (int16_t)q_multsr_sat_32x32(src[i + 6],
							  cd->volume[6],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
		dest[i + 7] = (int16_t)q_multsr_sat_32x32(src[i + 7],
							  cd->volume[7],
							  Q_SHIFT_BITS_64(31,
									  16,
									  15));
	}
}

/* copy and scale volume from 32 bit source buffer to 32 bit dest buffer */
static void vol_s32_to_s32_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		dest[i] = q_multsr_sat_32x32(src[i], cd->volume[0],
					     Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 1] = q_multsr_sat_32x32(src[i + 1], cd->volume[1],
						 Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 2] = q_multsr_sat_32x32(src[i + 2], cd->volume[2],
						 Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 3] = q_multsr_sat_32x32(src[i + 3], cd->volume[3],
						 Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 4] = q_multsr_sat_32x32(src[i + 4], cd->volume[4],
						 Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 5] = q_multsr_sat_32x32(src[i + 5], cd->volume[5],
						 Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 6] = q_multsr_sat_32x32(src[i + 6], cd->volume[6],
						 Q_SHIFT_BITS_64(31, 16, 31));
		dest[i + 7] = q_multsr_sat_32x32(src[i + 7], cd->volume[7],
						 Q_SHIFT_BITS_64(31, 16, 31));
	}
}

/* copy and scale volume from 16 bit source buffer to 16 bit dest buffer */
static void vol_s16_to_s16_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		dest[i] = q_multsr_sat_16x16(src[i], cd->volume[0],
					     Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 1] = q_multsr_sat_16x16(src[i + 1], cd->volume[1],
						 Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 2] = q_multsr_sat_16x16(src[i + 2], cd->volume[2],
						 Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 3] = q_multsr_sat_16x16(src[i + 3], cd->volume[3],
						 Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 4] = q_multsr_sat_16x16(src[i + 4], cd->volume[4],
						 Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 5] = q_multsr_sat_16x16(src[i + 5], cd->volume[5],
						 Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 6] = q_multsr_sat_16x16(src[i + 6], cd->volume[6],
						 Q_SHIFT_BITS_32(15, 16, 15));
		dest[i + 7] = q_multsr_sat_16x16(src[i + 7], cd->volume[7],
						 Q_SHIFT_BITS_32(15, 16, 15));
	}
}

/* copy and scale volume from 16 bit source buffer to 24 bit
 * on 32 bit boundary buffer
 */
static void vol_s16_to_s24_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		dest[i] = q_multsr_sat_32x32(src[i], cd->volume[0],
					     Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(src[i + 1], cd->volume[1],
						 Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 2] = q_multsr_sat_32x32(src[i + 2], cd->volume[2],
						 Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 3] = q_multsr_sat_32x32(src[i + 3], cd->volume[3],
						 Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 4] = q_multsr_sat_32x32(src[i + 4], cd->volume[4],
						 Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 5] = q_multsr_sat_32x32(src[i + 5], cd->volume[5],
						 Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 6] = q_multsr_sat_32x32(src[i + 6], cd->volume[6],
						 Q_SHIFT_BITS_64(15, 16, 23));
		dest[i + 7] = q_multsr_sat_32x32(src[i + 7], cd->volume[7],
						 Q_SHIFT_BITS_64(15, 16, 23));
	}
}

/* copy and scale volume from 16 bit source buffer to 24 bit
 * on 32 bit boundary dest buffer
 */
static void vol_s24_to_s16_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int32_t i, sample;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.15 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		sample = sign_extend_s24(src[i]);
		dest[i] = (int16_t)q_multsr_sat_32x32(sample, cd->volume[0],
						      Q_SHIFT_BITS_64(23, 16,
								      15));
		sample = sign_extend_s24(src[i + 1]);
		dest[i + 1] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[1],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
		sample = sign_extend_s24(src[i + 2]);
		dest[i + 2] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[2],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
		sample = sign_extend_s24(src[i + 3]);
		dest[i + 3] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[3],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
		sample = sign_extend_s24(src[i + 4]);
		dest[i + 4] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[4],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
		sample = sign_extend_s24(src[i + 5]);
		dest[i + 5] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[5],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
		sample = sign_extend_s24(src[i + 6]);
		dest[i + 6] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[6],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
		sample = sign_extend_s24(src[i + 7]);
		dest[i + 7] = (int16_t)q_multsr_sat_32x32(sample,
							  cd->volume[7],
							  Q_SHIFT_BITS_64(23,
									  16,
									  15));
	}
}

/* copy and scale volume from 32 bit source buffer to 24 bit
 * on 32 bit boundary dest buffer
 */
static void vol_s32_to_s24_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.31 --> Q1.23 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		dest[i] = q_multsr_sat_32x32(src[i], cd->volume[0],
					     Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(src[i + 1], cd->volume[1],
						 Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 2] = q_multsr_sat_32x32(src[i + 2], cd->volume[2],
						 Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 3] = q_multsr_sat_32x32(src[i + 3], cd->volume[3],
						 Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 4] = q_multsr_sat_32x32(src[i + 4], cd->volume[4],
						 Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 5] = q_multsr_sat_32x32(src[i + 5], cd->volume[5],
						 Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 6] = q_multsr_sat_32x32(src[i + 6], cd->volume[6],
						 Q_SHIFT_BITS_64(31, 16, 23));
		dest[i + 7] = q_multsr_sat_32x32(src[i + 7], cd->volume[7],
						 Q_SHIFT_BITS_64(31, 16, 23));
	}
}

/* copy and scale volume from 16 bit source buffer to 24 bit
 * on 32 bit boundary dest buffer
 */
static void vol_s24_to_s32_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int32_t i;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.31 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		dest[i] = q_multsr_sat_32x32(sign_extend_s24(src[i]),
					     cd->volume[0],
					     Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 1] = q_multsr_sat_32x32(sign_extend_s24(src[i + 1]),
						 cd->volume[1],
						 Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 2] = q_multsr_sat_32x32(sign_extend_s24(src[i + 2]),
						 cd->volume[2],
						 Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 3] = q_multsr_sat_32x32(sign_extend_s24(src[i + 3]),
						 cd->volume[3],
						 Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 4] = q_multsr_sat_32x32(sign_extend_s24(src[i + 4]),
						 cd->volume[4],
						 Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 5] = q_multsr_sat_32x32(sign_extend_s24(src[i + 5]),
						 cd->volume[5],
						 Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 6] = q_multsr_sat_32x32(sign_extend_s24(src[i + 6]),
						 cd->volume[6],
						 Q_SHIFT_BITS_64(23, 16, 31));
		dest[i + 7] = q_multsr_sat_32x32(sign_extend_s24(src[i + 7]),
						 cd->volume[7],
						 Q_SHIFT_BITS_64(23, 16, 31));
	}
}

/* Copy and scale volume from 24 bit source buffer to 24 bit on 32 bit boundary
 * dest buffer.
 */
static void vol_s24_to_s24_8ch(struct comp_dev *dev, struct comp_buffer *sink,
			       struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t i, *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;

	/* buffer sizes are always divisible by period frames */
	/* Samples are Q1.23 --> Q1.23 and volume is Q1.16 */
	for (i = 0; i < dev->frames * 8; i += 8) {
		dest[i] = q_multsr_sat_32x32(sign_extend_s24(src[i]),
					     cd->volume[0],
					     Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 1] = q_multsr_sat_32x32(sign_extend_s24(src[i + 1]),
						 cd->volume[1],
						 Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 2] = q_multsr_sat_32x32(sign_extend_s24(src[i + 2]),
						 cd->volume[2],
						 Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 3] = q_multsr_sat_32x32(sign_extend_s24(src[i + 3]),
						 cd->volume[3],
						 Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 4] = q_multsr_sat_32x32(sign_extend_s24(src[i + 4]),
						 cd->volume[4],
						 Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 5] = q_multsr_sat_32x32(sign_extend_s24(src[i + 5]),
						 cd->volume[5],
						 Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 6] = q_multsr_sat_32x32(sign_extend_s24(src[i + 6]),
						 cd->volume[6],
						 Q_SHIFT_BITS_64(23, 16, 23));
		dest[i + 7] = q_multsr_sat_32x32(sign_extend_s24(src[i + 7]),
						 cd->volume[7],
						 Q_SHIFT_BITS_64(23, 16, 23));
	}
}
#endif

const struct comp_func_map func_map[] = {
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, 2, vol_s16_to_s16_2ch},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, 2, vol_s16_to_s32_2ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, 2, vol_s32_to_s16_2ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, 2, vol_s32_to_s32_2ch},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, 2, vol_s16_to_s24_2ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, 2, vol_s24_to_s16_2ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, 2, vol_s32_to_s24_2ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, 2, vol_s24_to_s32_2ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, 2, vol_s24_to_s24_2ch},
#if PLATFORM_MAX_CHANNELS >= 4
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, 4, vol_s16_to_s16_4ch},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, 4, vol_s16_to_s32_4ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, 4, vol_s32_to_s16_4ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, 4, vol_s32_to_s32_4ch},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, 4, vol_s16_to_s24_4ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, 4, vol_s24_to_s16_4ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, 4, vol_s32_to_s24_4ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, 4, vol_s24_to_s32_4ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, 4, vol_s24_to_s24_4ch},
#endif
#if PLATFORM_MAX_CHANNELS >= 8
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, 8, vol_s16_to_s16_8ch},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, 8, vol_s16_to_s32_8ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, 8, vol_s32_to_s16_8ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, 8, vol_s32_to_s32_8ch},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, 8, vol_s16_to_s24_8ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, 8, vol_s24_to_s16_8ch},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, 8, vol_s32_to_s24_8ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, 8, vol_s24_to_s32_8ch},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, 8, vol_s24_to_s24_8ch},
#endif
};

scale_vol vol_get_processing_function(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i;

	/* map the volume function for source and sink buffers */
	for (i = 0; i < ARRAY_SIZE(func_map); i++) {
		if (cd->source_format != func_map[i].source)
			continue;
		if (cd->sink_format != func_map[i].sink)
			continue;
		if (dev->params.channels != func_map[i].channels)
			continue;

		return func_map[i].func;
	}

	return NULL;
}

#endif

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
 * \brief Volume gain function
 * \param[in] x   input sample.
 * \param[in] vol gain.
 * \return output sample.
 *
 * Volume multiply for 24 bit input and 16 bit bit output.
 */
static inline int16_t vol_mult_s24_to_s16(int32_t x, int32_t vol)
{
	return (int16_t)q_multsr_sat_32x32_16(sign_extend_s24(x), vol,
					      Q_SHIFT_BITS_64(23, 16, 15));
}

/**
 * \brief Volume s32 to s16 multiply function
 * \param[in] x   input sample.
 * \param[in] vol gain.
 * \return output sample.
 *
 * Volume multiply for 32 bit input and 16 bit bit output.
 */
static inline int16_t vol_mult_s32_to_s16(int32_t x, int32_t vol)
{
	return (int16_t)q_multsr_sat_32x32_16(x, vol,
					      Q_SHIFT_BITS_64(31, 16, 15));
}

/**
 * \brief Volume s16 to s24 multiply function
 * \param[in] x   input sample.
 * \param[in] vol gain.
 * \return output sample.
 *
 * Volume multiply for 16 bit input and 24 bit bit output.
 */
static inline int32_t vol_mult_s16_to_s24(int16_t x, int32_t vol)
{
	return q_multsr_sat_32x32_24(x, vol, Q_SHIFT_BITS_64(15, 16, 23));
}

/**
 * \brief Volume s24 to s24 multiply function
 * \param[in] x   input sample.
 * \param[in] vol gain.
 * \return output sample.
 *
 * Volume multiply for 24 bit input and 24 bit bit output.
 */
static inline int32_t vol_mult_s24_to_s24(int32_t x, int32_t vol)
{
	return q_multsr_sat_32x32_24(sign_extend_s24(x), vol,
				     Q_SHIFT_BITS_64(23, 16, 23));
}

/**
 * \brief Volume s32 to s24 multiply function
 * \param[in] x   input sample.
 * \param[in] vol gain.
 * \return output sample.
 *
 * Volume multiply for 32 bit input and 24 bit bit output.
 */
static inline int32_t vol_mult_s32_to_s24(int32_t x, int32_t vol)
{
	return q_multsr_sat_32x32_24(x, vol, Q_SHIFT_BITS_64(31, 16, 23));
}

/**
 * \brief Volume processing from 16 bit to 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 32 bit destination buffer.
 */
static void vol_s16_to_s32(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src;
	int32_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.15 --> Q1.31 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s16(source, buff_frag);
			dest = buffer_write_frag_s32(sink, buff_frag);
			*dest = q_multsr_sat_32x32
				(*src << 8, cd->volume[channel],
				 Q_SHIFT_BITS_64(23, 16, 31));


			buff_frag++;
		}
	}
}

/**
 * \brief Volume processing from 32 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 16 bit destination buffer.
 */
static void vol_s32_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src;
	int16_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.31 --> Q1.15 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s32(source, buff_frag);
			dest = buffer_write_frag_s16(sink, buff_frag);

			*dest = vol_mult_s32_to_s16(*src, cd->volume[channel]);

			buff_frag++;
		}
	}
}

/**
 * \brief Volume processing from 32 bit to 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 32 bit destination buffer.
 */
static void vol_s32_to_s32(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src;
	int32_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.31 --> Q1.31 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s32(source, buff_frag);
			dest = buffer_write_frag_s32(sink, buff_frag);

			*dest = q_multsr_sat_32x32
				(*src, cd->volume[channel],
				 Q_SHIFT_BITS_64(31, 16, 31));

			buff_frag++;
		}
	}
}

/**
 * \brief Volume processing from 16 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 16 bit destination buffer.
 */
static void vol_s16_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src;
	int16_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.15 --> Q1.15 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s16(source, buff_frag);
			dest = buffer_write_frag_s16(sink, buff_frag);

			*dest = q_multsr_sat_32x32_16
				(*src, cd->volume[channel],
				 Q_SHIFT_BITS_32(15, 16, 15));

			buff_frag++;
		}
	}
}

/**
 * \brief Volume processing from 16 bit to 24/32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 16 bit source buffer
 * to 24/32 bit destination buffer.
 */
static void vol_s16_to_s24(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src;
	int32_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.15 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s16(source, buff_frag);
			dest = buffer_write_frag_s32(sink, buff_frag);

			*dest = vol_mult_s16_to_s24(*src, cd->volume[channel]);

			buff_frag++;
		}
	}
}

/**
 * \brief Volume processing from 24/32 bit to 16 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 16 bit destination buffer.
 */
static void vol_s24_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src;
	int16_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.23 --> Q1.15 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s32(source, buff_frag);
			dest = buffer_write_frag_s16(sink, buff_frag);

			*dest = vol_mult_s24_to_s16(*src, cd->volume[channel]);

			buff_frag++;
		}
	}
}

/**
 * \brief Volume processing from 32 bit to 24/32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 32 bit source buffer
 * to 24/32 bit destination buffer.
 */
static void vol_s32_to_s24(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src;
	int32_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.31 --> Q1.23 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s32(source, buff_frag);
			dest = buffer_write_frag_s32(sink, buff_frag);

			*dest = vol_mult_s32_to_s24(*src, cd->volume[channel]);

			buff_frag++;
		}
	}
}

/**
 * \brief Volume processing from 24/32 bit to 32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 32 bit destination buffer.
 */
static void vol_s24_to_s32(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src;
	int32_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.23 --> Q1.31 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s32(source, buff_frag);
			dest = buffer_write_frag_s32(sink, buff_frag);

			*dest = q_multsr_sat_32x32
				(sign_extend_s24(*src), cd->volume[channel],
				 Q_SHIFT_BITS_64(23, 16, 31));

			buff_frag++;
		}
	}
}

/**
 * \brief Volume processing from 24/32 bit to 24/32 bit.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 *
 * Copy and scale volume from 24/32 bit source buffer
 * to 24/32 bit destination buffer.
 */
static void vol_s24_to_s24(struct comp_dev *dev, struct comp_buffer *sink,
			   struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src;
	int32_t *dest;
	int32_t i;
	uint32_t channel;
	uint32_t buff_frag = 0;

	/* Samples are Q1.23 --> Q1.23 and volume is Q8.16 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s32(source, buff_frag);
			dest = buffer_write_frag_s32(sink, buff_frag);

			*dest = vol_mult_s24_to_s24(*src, cd->volume[channel]);

			buff_frag++;
		}
	}
}

const struct comp_func_map func_map[] = {
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, vol_s16_to_s16},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, vol_s16_to_s32},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, vol_s32_to_s16},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, vol_s32_to_s32},
	{SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, vol_s16_to_s24},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, vol_s24_to_s16},
	{SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, vol_s32_to_s24},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, vol_s24_to_s32},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, vol_s24_to_s24},
};

const size_t func_count = ARRAY_SIZE(func_map);

#endif

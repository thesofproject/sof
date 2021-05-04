// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file audio/pcm_converter/pcm_converter_hifi3.c
 * \brief PCM converter HiFi3 processing implementation
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 * \authors Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#include <sof/audio/pcm_converter.h>

#ifdef PCM_CONVERTER_HIFI3

#include <sof/audio/buffer.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <xtensa/tie/xt_hifi3.h>

#include <stddef.h>
#include <stdint.h>

#if XCHAL_HAVE_FP
#include <xtensa/tie/xt_FP.h>
#endif

/**
 * \brief Sets buffer to be circular using HiFi3 functions.
 * \param[in,out] buffer Circular buffer.
 */
static void pcm_converter_setup_circular(const struct audio_stream *source)
{
	AE_SETCBEGIN0(source->addr);
	AE_SETCEND0(source->end_addr);
}

#if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE

/**
 * \brief HiFi3 enabled PCM conversion from 16 bit to 24 bit.
 * \param[in,out] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 */
static int pcm_convert_s16_to_s24(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	ae_int16 *in = audio_stream_read_frag(source, ioffset, sizeof(int16_t));
	ae_int32 *out = audio_stream_write_frag(sink, ooffset, sizeof(int32_t));
	ae_int16x4 sample = AE_ZERO16();
	ae_valign align_out = AE_ZALIGN64();
	ae_int16x4 *in16x4;
	ae_int32x2 *out32x2;
	int i = 0;

	/* nothing to do */
	if (!samples)
		return samples;

	/* required alignment for AE_L16X4_XC */
	while (!IS_ALIGNED((uintptr_t)in, 8)) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load one 16 bit sample */
		AE_L16_XC(sample, in, sizeof(ae_int16));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift right and store one 32 bit sample */
		AE_S32_L_XC(AE_SRAI32(AE_CVT32X2F16_32(sample), 8), out,
			    sizeof(ae_int32));

		if (++i == samples)
			return samples;
	}

	in16x4 = (ae_int16x4 *)in;
	out32x2 = (ae_int32x2 *)out;

	/* main loop processes 4 samples at a time */
	while (samples >= 3 && i < samples - 3) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load four 16 bit samples */
		AE_L16X4_XC(sample, in16x4, sizeof(ae_int16x4));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift right and store four 32 bit samples */
		AE_SA32X2_IC(AE_SRAI32(AE_CVT32X2F16_32(sample), 8), align_out,
			     out32x2);
		AE_SA32X2_IC(AE_SRAI32(AE_CVT32X2F16_10(sample), 8), align_out,
			     out32x2);

		i += 4;
	}

	/* flush align_out register to memory */
	AE_SA64POS_FC(align_out, out32x2);

	in = (ae_int16 *)in16x4;
	out = (ae_int32 *)out32x2;

	while (i++ != samples) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load one 16 bit sample */
		AE_L16_XC(sample, in, sizeof(ae_int16));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift right and store one 32 bit sample */
		AE_S32_L_XC(AE_SRAI32(AE_CVT32X2F16_32(sample), 8), out,
			    sizeof(ae_int32));
	}

	return samples;
}

/**
 * \brief HiFi3 enabled PCM shift from 24 bit to 16 bit.
 * \param[in] sample Input sample.
 * \return Shifted sample.
 */
static ae_int32x2 pcm_shift_s24_to_s16(ae_int32x2 sample)
{
	/* shift with saturation and rounding */
	sample = AE_SLAA32(sample, 8);
	sample = AE_SRAI32R(sample, 16);
	sample = AE_SLAI32S(sample, 16);
	sample = AE_SRAI32(sample, 16);

	return sample;
}

/**
 * \brief HiFi3 enabled PCM conversion from 24 bit to 16 bit.
 * \param[in,out] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_s24_to_s16(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	ae_int32x2 *in = audio_stream_read_frag(source, ioffset,
						sizeof(int32_t));
	ae_int16x4 *out = audio_stream_write_frag(sink, ooffset,
						  sizeof(int16_t));
	ae_int16x4 sample = AE_ZERO16();
	ae_int32x2 sample_1 = AE_ZERO32();
	ae_int32x2 sample_2 = AE_ZERO32();
	ae_valign align_out = AE_ZALIGN64();
	int i = 0;
	int leftover;

	/* nothing to do */
	if (!samples)
		return samples;

	/* required alignment for AE_L32X2_XC */
	if (!IS_ALIGNED((uintptr_t)in, 8)) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load one 32 bit sample */
		AE_L32_XC(sample_1, (ae_int32 *)in, sizeof(ae_int32));

		/* shift and round */
		sample_1 = pcm_shift_s24_to_s16(sample_1);
		sample = AE_MOVINT16X4_FROMINT32X2(sample_1);

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* store one 16 bit sample */
		AE_S16_0_XC(AE_MOVAD16_0(sample), (ae_int16 *)out,
			    sizeof(ae_int16));

		samples--;
	}

	/* main loop processes 4 samples at a time */
	while (samples >= 3 && i < samples - 3) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load four 32 bit samples */
		AE_L32X2_XC(sample_1, in, sizeof(ae_int32x2));
		AE_L32X2_XC(sample_2, in, sizeof(ae_int32x2));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift and round */
		sample_1 = pcm_shift_s24_to_s16(sample_1);
		sample_2 = pcm_shift_s24_to_s16(sample_2);

		/* store four 16 bit samples */
		sample = AE_CVT16X4(sample_1, sample_2);
		AE_SA16X4_IC(sample, align_out, out);

		i += 4;
	}

	/* flush align_out register to memory */
	AE_SA64POS_FC(align_out, out);

	leftover = samples - i;

	while (leftover) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load two 32 bit samples */
		AE_L32X2_XC(sample_1, in, sizeof(ae_int32x2));

		/* shift and round */
		sample_1 = pcm_shift_s24_to_s16(sample_1);
		sample = AE_MOVINT16X4_FROMINT32X2(sample_1);

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* store one 16 bit sample */
		AE_S16_0_XC(AE_MOVAD16_2(sample), (ae_int16 *)out,
			    sizeof(ae_int16));

		/* no more samples to process */
		if (leftover == 1)
			return samples;

		/* store one 16 bit sample */
		AE_S16_0_XC(AE_MOVAD16_0(sample), (ae_int16 *)out,
			    sizeof(ae_int16));

		leftover -= 2;
	}

	return samples;
}

#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE */

#if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE

/**
 * \brief HiFi3 enabled PCM conversion from 16 bit to 32 bit.
 * \param[in,out] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_s16_to_s32(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	ae_int16 *in = audio_stream_read_frag(source, ioffset,
					      sizeof(int16_t));
	ae_int32 *out = audio_stream_write_frag(sink, ooffset,
						sizeof(int32_t));
	ae_int16x4 sample = AE_ZERO16();
	ae_valign align_out = AE_ZALIGN64();
	ae_int16x4 *in16x4;
	ae_int32x2 *out32x2;
	int i = 0;

	/* nothing to do */
	if (!samples)
		return samples;

	/* required alignment for AE_L16X4_XC */
	while (!IS_ALIGNED((uintptr_t)in, 8)) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load one 16 bit sample */
		AE_L16_XC(sample, in, sizeof(ae_int16));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* store one 32 bit sample */
		AE_S32_L_XC(AE_CVT32X2F16_32(sample), out, sizeof(ae_int32));

		if (++i == samples)
			return samples;
	}

	in16x4 = (ae_int16x4 *)in;
	out32x2 = (ae_int32x2 *)out;

	/* main loop processes 4 samples at a time */
	while (samples >= 3 && i < samples - 3) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load four 16 bit samples */
		AE_L16X4_XC(sample, in16x4, sizeof(ae_int16x4));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* store two 32 bit samples */
		AE_SA32X2_IC(AE_CVT32X2F16_32(sample), align_out, out32x2);
		AE_SA32X2_IC(AE_CVT32X2F16_10(sample), align_out, out32x2);

		i += 4;
	}

	/* flush align_out register to memory */
	AE_SA64POS_FC(align_out, out32x2);

	in = (ae_int16 *)in16x4;
	out = (ae_int32 *)out32x2;

	while (i++ != samples) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load one 16 bit sample */
		AE_L16_XC(sample, in, sizeof(ae_int16));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* store one 32 bit sample */
		AE_S32_L_XC(AE_CVT32X2F16_32(sample), out, sizeof(ae_int32));
	}

	return samples;
}

/**
 * \brief HiFi3 enabled PCM conversion from 32 bit to 16 bit.
 * \param[in,out] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_s32_to_s16(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	ae_int32x2 *in = audio_stream_read_frag(source, ioffset,
						sizeof(int32_t));
	ae_int16x4 *out = audio_stream_write_frag(sink, ooffset,
						  sizeof(int16_t));
	ae_int16x4 sample = AE_ZERO16();
	ae_int32x2 sample_1 = AE_ZERO32();
	ae_int32x2 sample_2 = AE_ZERO32();
	ae_valign align_out = AE_ZALIGN64();
	int i = 0;
	int leftover;

	/* nothing to do */
	if (!samples)
		return samples;

	/* required alignment for AE_L32X2_XC */
	if (!IS_ALIGNED((uintptr_t)in, 8)) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load one 32 bit sample */
		AE_L32_XC(sample_1, (ae_int32 *)in, sizeof(ae_int32));

		/* shift and round */
		sample = AE_ROUND16X4F32SSYM(sample_1, sample_1);

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* store one 16 bit sample */
		AE_S16_0_XC(AE_MOVAD16_0(sample), (ae_int16 *)out,
			    sizeof(ae_int16));

		samples--;
	}

	/* main loop processes 4 samples at a time */
	while (samples >= 3 && i < samples - 3) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load four 32 bit samples */
		AE_L32X2_XC(sample_1, in, sizeof(ae_int32x2));
		AE_L32X2_XC(sample_2, in, sizeof(ae_int32x2));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift and round */
		sample = AE_ROUND16X4F32SSYM(sample_1, sample_2);

		/* store four 16 bit samples */
		AE_SA16X4_IC(sample, align_out, out);

		i += 4;
	}

	/* flush align_out register to memory */
	AE_SA64POS_FC(align_out, out);

	leftover = samples - i;

	while (leftover) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load two 32 bit samples */
		AE_L32X2_XC(sample_1, in, sizeof(ae_int32x2));

		/* shift and round */
		sample = AE_ROUND16X4F32SSYM(sample_1, sample_1);

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* store one 16 bit sample */
		AE_S16_0_XC(AE_MOVAD16_1(sample), (ae_int16 *)out,
			    sizeof(ae_int16));

		/* no more samples to process */
		if (leftover == 1)
			return samples;

		/* store one 16 bit sample */
		AE_S16_0_XC(AE_MOVAD16_0(sample), (ae_int16 *)out,
			    sizeof(ae_int16));

		leftover -= 2;
	}

	return samples;
}

#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE */

#if CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE

/**
 * \brief HiFi3 enabled PCM conversion from 24 bit to 32 bit.
 * \param[in,out] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_s24_to_s32(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	ae_int32x2 *in = audio_stream_read_frag(source, ioffset,
						sizeof(int32_t));
	ae_int32x2 *out = audio_stream_write_frag(sink, ooffset,
						  sizeof(int32_t));
	ae_int32x2 sample = AE_ZERO32();
	ae_valign align_out = AE_ZALIGN64();
	int i;

	/* nothing to do */
	if (!samples)
		return samples;

	/* required alignment for AE_L32X2_XC */
	if (!IS_ALIGNED((uintptr_t)in, 8)) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load one 32 bit sample */
		AE_L32_XC(sample, (ae_int32 *)in, sizeof(ae_int32));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift left and store one 32 bit sample */
		AE_S32_L_XC(AE_SLAI32(sample, 8), (ae_int32 *)out,
			    sizeof(ae_int32));

		samples--;
	}

	/* main loop processes 2 samples at a time */
	for (i = 0; i < samples - 1; i += 2) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load two 32 bit samples */
		AE_L32X2_XC(sample, in, sizeof(ae_int32x2));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift left and store two 32 bit samples */
		AE_SA32X2_IC(AE_SLAI32(sample, 8), align_out, out);
	}

	/* flush align_out register to memory */
	AE_SA64POS_FC(align_out, out);

	/* no more samples to process */
	if (i == samples)
		return samples;

	/* set source as circular buffer */
	pcm_converter_setup_circular(source);

	/* load one 32 bit sample */
	AE_L32_XC(sample, (ae_int32 *)in, 0);

	/* set sink as circular buffer */
	pcm_converter_setup_circular(sink);

	/* shift left and store one 32 bit sample */
	AE_S32_L_XC(AE_SLAI32(sample, 8), (ae_int32 *)out, 0);

	return samples;
}

/**
 * \brief HiFi3 enabled PCM shift from 32 bit to 24 bit.
 * \param[in] sample Input sample.
 * \return Shifted sample.
 */
static ae_int32x2 pcm_shift_s32_to_s24(ae_int32x2 sample)
{
	/* shift with saturation and rounding */
	sample = AE_SRAI32R(sample, 8);
	sample = AE_SLAI32S(sample, 8);
	sample = AE_SRAI32(sample, 8);

	return sample;
}

/**
 * \brief HiFi3 enabled PCM conversion from 32 bit to 24 bit.
 * \param[in,out] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_s32_to_s24(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	ae_int32x2 *in = audio_stream_read_frag(source, ioffset,
						sizeof(int32_t));
	ae_int32x2 *out = audio_stream_write_frag(sink, ooffset,
						  sizeof(int32_t));
	ae_int32x2 sample = AE_ZERO32();
	ae_valign align_out = AE_ZALIGN64();
	int i;

	/* nothing to do */
	if (!samples)
		return samples;

	/* required alignment for AE_L32X2_XC */
	if (!IS_ALIGNED((uintptr_t)in, 8)) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load one 32 bit sample */
		AE_L32_XC(sample, (ae_int32 *)in, sizeof(ae_int32));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift right and store one 32 bit sample */
		sample = pcm_shift_s32_to_s24(sample);
		AE_S32_L_XC(sample, (ae_int32 *)out, sizeof(ae_int32));

		samples--;
	}

	/* main loop processes 2 samples at a time */
	for (i = 0; i < samples - 1; i += 2) {
		/* set source as circular buffer */
		pcm_converter_setup_circular(source);

		/* load two 32 bit samples */
		AE_L32X2_XC(sample, in, sizeof(ae_int32x2));

		/* set sink as circular buffer */
		pcm_converter_setup_circular(sink);

		/* shift right and store two 32 bit samples */
		sample = pcm_shift_s32_to_s24(sample);
		AE_SA32X2_IC(sample, align_out, out);
	}

	/* flush align_out register to memory */
	AE_SA64POS_FC(align_out, out);

	/* no more samples to process */
	if (i == samples)
		return samples;

	/* set source as circular buffer */
	pcm_converter_setup_circular(source);

	/* load one 32 bit sample */
	AE_L32_XC(sample, (ae_int32 *)in, 0);

	/* set sink as circular buffer */
	pcm_converter_setup_circular(sink);

	/* shift right and store one 32 bit sample */
	sample = pcm_shift_s32_to_s24(sample);
	AE_S32_L_XC(sample, (ae_int32 *)out, 0);

	return samples;
}

#endif /* CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE */

#if XCHAL_HAVE_FP

static inline int _round_s(xtfloat x)
{
#if defined(XT_ROUND_S)
	return XT_ROUND_S(x, 15);
#else
	return XT_TRUNC_S(XT_FIROUND_S(x), 15);
#endif
}

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE

/**
 * \brief HiFi3 enabled PCM conversion from 16 bit to IEEE-754 float.
 * \param[in] psrc source linear buffer.
 * \param[out] pdst destination linear buffer.
 * \param[in] samples Number of samples to process.
 */
static void pcm_convert_s16_to_f_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const ae_int16 *in = psrc;
	xtfloat *out = pdst;
	ae_int16x4 sample = AE_ZERO16();
	xtfloat fl;
	int i = 0;

	while (i < samples) {
		/* load one 16 bit sample */
		AE_L16_XC(sample, in, sizeof(ae_int16));

		/* run conversion */
		fl = XT_FLOAT_S((ae_int16)sample, 15);

		/* store one float sample */
		/* need address align to 32 bits */
		XT_xtfloat_storeip(fl, out, sizeof(fl));

		++i;
	}
}

/**
 * \brief HiFi3 enabled PCM conversion from IEEE-754 float to 16 bit.
 * \param[in] psrc source linear buffer.
 * \param[out] pdst destination linear buffer.
 * \param[in] samples Number of samples to process.
 */
static void pcm_convert_f_to_s16_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const xtfloat *in = psrc;
	ae_int16x4 *out = pdst;
	xtfloat x;
	int y;
	int i = 0;

	while (i < samples) {
		/* load one 32 bit sample */
		XT_xtfloat_loadip(x, in, sizeof(x));

		/* shift and round */
		y = _round_s(x);

		/* store one 16 bit sample */
		AE_S16_0_IP(y, (ae_int16 *)out, sizeof(ae_int16));

		++i;
	}
}

/**
 * \brief HiFi3 enabled PCM conversion from 16 bit to IEEE-754 float.
 * \param[in] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_s16_to_f(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_s16_to_f_lin);
}

/**
 * \brief HiFi3 enabled PCM conversion from IEEE-754 float to 16 bit.
 * \param[in] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_f_to_s16(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_f_to_s16_lin);
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE

/**
 * \brief HiFi3 enabled PCM conversion from  24 bit to IEEE-754 float.
 * \param[in] psrc source linear buffer.
 * \param[out] pdst destination linear buffer.
 * \param[in] samples Number of samples to process.
 */
static void pcm_convert_s24_to_f_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const ae_int32 *in = psrc;
	xtfloat *out = pdst;
	ae_int32x2 sample = AE_ZERO32();
	xtfloat fl;
	const xtfloat ratio = (xtfloat)(1.f / (1 << (23 - 15)));
	int i = 0;

	while (i < samples) {
		/* load one 24 bit sample */
		AE_L32_XC(sample, in, sizeof(*in));

		/* extend sign */
		sample = AE_SRAI32(AE_SLAI32(sample, 8), 8);
		/* run conversion */
		fl = XT_FLOAT_S(sample, 15);
		fl = fl * ratio;

		/* store one 32 bit float sample */
		/* need address align to 32 bits */
		XT_xtfloat_storeip(fl, out, sizeof(fl));

		++i;
	}
}

/**
 * \brief HiFi3 enabled PCM conversion from IEEE-754 float to 24 bit.
 * \param[in] psrc source linear buffer.
 * \param[out] pdst destination linear buffer.
 * \param[in] samples Number of samples to process.
 */
static void pcm_convert_f_to_s24_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const xtfloat *in = psrc;
	ae_int32 *out = pdst;
	xtfloat x;
	ae_int32x2 y1;
	int i = 0;
	const xtfloat ratio = (xtfloat)(1 << (23 - 15));

	while (i < samples) {
		/* load one 32 bit sample */
		/* need address align to 32 bits */
		XT_xtfloat_loadxp(x, in, sizeof(x));

		/* shift and round */
		x = x * ratio;
		y1 = _round_s(x);

		y1 = AE_SAT24S((ae_f32x2)y1);

		/* store one 24 bit sample in 32 bit memory size */
		AE_S32_L_IP(y1, (ae_int32 *)out, sizeof(*out));

		++i;
	}
}

/**
 * \brief HiFi3 enabled PCM conversion from 24 bit to IEEE-754 float.
 * \param[in] psrc source linear buffer.
 * \param[out] pdst destination linear buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_s24_to_f(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_s24_to_f_lin);
}

/**
 * \brief HiFi3 enabled PCM conversion from IEEE-754 float to 24 bit.
 * \param[in] psrc source linear buffer.
 * \param[out] pdst destination linear buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_f_to_s24(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_f_to_s24_lin);
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_24LE */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE

/**
 * \brief HiFi3 enabled PCM conversion from 32 bit to IEEE-754 float.
 * \param[in] psrc source linear buffer.
 * \param[out] pdst destination linear buffer.
 * \param[in] samples Number of samples to process.
 */
static void pcm_convert_s32_to_f_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const ae_int32 *in = psrc;
	xtfloat *out = pdst;
	ae_int32x2 sample = AE_ZERO32();
	xtfloat fl;
	const xtfloat ratio = (xtfloat)(1.f / (1ul << (31 - 15)));
	int i = 0;

	while (i < samples) {
		/* load one 32 bit sample */
		AE_L32_XC(sample, in, sizeof(*in));

		/* run conversion */
		fl = XT_FLOAT_S(sample, 15);
		fl = fl * ratio;

		/* store one 32 bit float sample */
		/* need address align to 32 bits */
		XT_xtfloat_storeip(fl, out, sizeof(fl));

		++i;
	}
}

/**
 * \brief HiFi3 enabled PCM conversion from IEEE-754 float to 32 bit.
 * \param[in] psrc source linear buffer.
 * \param[out] pdst destination linear buffer.
 * \param[in] samples Number of samples to process.
 */
static void pcm_convert_f_to_s32_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const xtfloat *in = psrc;
	ae_int32 *out = pdst;
	xtfloat x;
	ae_int32x2 y1;
	int i = 0;
	const xtfloat ratio = (xtfloat)(1ul << (31 - 15));

	while (i < samples) {
		/* load one 32 bit sample */
		/* need address align to 32 bits */
		XT_xtfloat_loadxp(x, in, sizeof(x));

		/* shift and round */
		x = x * ratio;
		y1 = _round_s(x);

		/* store one 32 bit sample */
		AE_S32_L_IP(y1, (ae_int32 *)out, sizeof(*out));

		++i;
	}
}

/**
 * \brief HiFi3 enabled PCM conversion from 32 bit to IEEE-754 float.
 * \param[in] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_s32_to_f(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_s32_to_f_lin);
}

/**
 * \brief HiFi3 enabled PCM conversion from IEEE-754 float to 32 bit.
 * \param[in] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 * \return error code or number of processed samples.
 */
static int pcm_convert_f_to_s32(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_f_to_s32_lin);
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_32LE */
#endif /* XCHAL_HAVE_FP */

const struct pcm_func_map pcm_func_map[] = {
#if CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S24LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, pcm_convert_s16_to_s24 },
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, pcm_convert_s24_to_s16 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S32LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, pcm_convert_s16_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, pcm_convert_s32_to_s16 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, pcm_convert_s24_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, pcm_convert_s32_to_s24 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE */
#if XCHAL_HAVE_FP
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_FLOAT, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT */
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s16_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S16_LE, pcm_convert_f_to_s16 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE */
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s24_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S24_4LE, pcm_convert_f_to_s24 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE */
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s32_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE, pcm_convert_f_to_s32 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE */
#endif /* XCHAL_HAVE_FP */
};

const size_t pcm_func_count = ARRAY_SIZE(pcm_func_map);

#endif

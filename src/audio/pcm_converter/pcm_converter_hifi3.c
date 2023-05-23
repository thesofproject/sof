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
#include <sof/compiler_attributes.h>
#include <ipc/stream.h>
#include <xtensa/tie/xt_hifi3.h>

#include <stddef.h>
#include <stdint.h>

#if XCHAL_HAVE_FP
#include <xtensa/tie/xt_FP.h>
#endif

#if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE

/**
 * \brief HiFi3 enabled PCM conversion from 16 bit to 24 bit.
 * \param[in,out] source Source buffer.
 * \param[in,out] sink Destination buffer.
 * \param[in] samples Number of samples to process.
 */
static int pcm_convert_s16_to_s24(const struct audio_stream __sparse_cache *source,
				  uint32_t ioffset, struct audio_stream __sparse_cache *sink,
				  uint32_t ooffset, uint32_t samples)
{
	ae_int16x4 sample = AE_ZERO16();
	uint32_t nmax, i, n, m, left, left_samples;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	int16_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;

	ae_int16x4 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s16(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 2;
		left = n & 0x03;
		inu = AE_LA64_PP(in);

		for (i = 0; i < m; i++) {
			/* load four 16 bit samples */
			AE_LA16X4_IP(sample, inu, in);
			/* shift right and store four 32 bit samples */
			AE_SA32X2_IP(AE_SRAI32(AE_CVT32X2F16_32(sample), 8), outu, out);
			AE_SA32X2_IP(AE_SRAI32(AE_CVT32X2F16_10(sample), 8), outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			/* load one 16 bit samples */
			AE_L16_IP(sample, (ae_int16 *)in, sizeof(ae_int16));

			/* shift right and store one 32 bit sample */
			AE_S32_L_IP(AE_SRAI32(AE_CVT32X2F16_32(sample), 8), (ae_int32 *)out,
				    sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink,  out);
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
static int pcm_convert_s24_to_s16(const struct audio_stream __sparse_cache *source,
				  uint32_t ioffset, struct audio_stream __sparse_cache *sink,
				  uint32_t ooffset, uint32_t samples)
{
	ae_int16x4 sample = AE_ZERO16();
	ae_int32x2 sample_1 = AE_ZERO32();
	ae_int32x2 sample_2 = AE_ZERO32();
	uint32_t nmax, i, n, m, left, left_samples;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	ae_int32 *src = source->r_ptr;
	ae_int16 *dst = sink->w_ptr;

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int16x4 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(n, nmax);
		m = n >> 2;
		left = n & 0x03;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load four 32 bit samples */
			AE_LA32X2_IP(sample_1, inu, in);
			AE_LA32X2_IP(sample_2, inu, in);

			sample_1 = pcm_shift_s24_to_s16(sample_1);
			sample_2 = pcm_shift_s24_to_s16(sample_2);

			/* store four 16 bit samples */
			sample = AE_CVT16X4(sample_1, sample_2);
			AE_SA16X4_IP(sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			/* load one 32 bit sample */
			AE_L32_IP(sample_1, (ae_int32 *)in, sizeof(ae_int32));

			/* shift and round */
			sample_1 = pcm_shift_s24_to_s16(sample_1);
			sample = AE_MOVINT16X4_FROMINT32X2(sample_1);

			/* store one 16 bit sample */
			AE_S16_0_IP(AE_MOVAD16_0(sample), (ae_int16 *)out,
				    sizeof(ae_int16));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
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
static int pcm_convert_s16_to_s32(const struct audio_stream __sparse_cache *source,
				  uint32_t ioffset, struct audio_stream __sparse_cache *sink,
				  uint32_t ooffset, uint32_t samples)
{
	int16_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int16x4 sample = AE_ZERO16();
	uint32_t nmax, i, n, m, left, left_samples;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();

	ae_int16x4 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s16(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 2;
		left = n & 0x03;

		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load four 16 bit samples */
			AE_LA16X4_IP(sample, inu, in);
			/* shift left and store four 32 bit samples */
			AE_SA32X2_IP(AE_CVT32X2F16_32(sample), outu, out);
			AE_SA32X2_IP(AE_CVT32X2F16_10(sample), outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			/* load one 16 bit samples */
			AE_L16_IP(sample, (ae_int16 *)in, sizeof(ae_int16));

			/* store one 32 bit sample */
			AE_S32_L_IP(AE_CVT32X2F16_32(sample), (ae_int32 *)out, sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
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
static int pcm_convert_s32_to_s16(const struct audio_stream __sparse_cache *source,
				  uint32_t ioffset, struct audio_stream __sparse_cache *sink,
				  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int16_t *dst = sink->w_ptr;
	ae_int16x4 sample = AE_ZERO16();
	ae_int32x2 sample_1 = AE_ZERO32();
	ae_int32x2 sample_2 = AE_ZERO32();
	uint32_t nmax, i, n, m, left, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int16x4 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(n, nmax);
		m = n >> 2;
		left = n & 0x3;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load four 32 bit samples */
			AE_LA32X2_IP(sample_1, inu, in);
			AE_LA32X2_IP(sample_2, inu, in);
			/* shift and round */
			sample = AE_ROUND16X4F32SSYM(sample_1, sample_2);
			/* store four 16 bit samples */
			AE_SA16X4_IP(sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			/* load one 32 bit samples */
			AE_L32_IP(sample_1, (ae_int32 *)in, sizeof(ae_int32));
			/* shift and round */
			sample = AE_ROUND16X4F32SSYM(sample_1, sample_1);
			/* store one 16 bit sample */
			AE_S16_0_IP(AE_MOVAD16_0(sample), (ae_int16 *)out,
				    sizeof(ae_int16));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
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
static int pcm_convert_s24_to_s32(const struct audio_stream __sparse_cache *source,
				  uint32_t ioffset, struct audio_stream __sparse_cache *sink,
				  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int32x2 sample = AE_ZERO32();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA32X2_IP(sample, inu, in);
			/* shift left and store two 32 bit samples */
			AE_SA32X2_IP(AE_SLAI32(sample, 8), outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_S32_L_IP(AE_SLAI32(sample, 8), (ae_int32 *)out,
				    sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

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
static int pcm_convert_s32_to_s24(const struct audio_stream __sparse_cache *source,
				  uint32_t ioffset, struct audio_stream __sparse_cache *sink,
				  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int32x2 sample = AE_ZERO32();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA32X2_IP(sample, inu, in);
			sample = pcm_shift_s32_to_s24(sample);
			AE_SA32X2_IP(sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			/* load one 32 bit sample */
			AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
			/* shift right and store one 32 bit sample */
			sample = pcm_shift_s32_to_s24(sample);
			AE_S32_L_IP(sample, (ae_int32 *)out, sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

	return samples;
}

static int pcm_convert_s32_to_s24_be(const struct audio_stream __sparse_cache *source,
				     uint32_t ioffset, struct audio_stream __sparse_cache *sink,
				     uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int32x2 sample = AE_ZERO32();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA32X2_IP(sample, inu, in);
			/* shift with saturation and rounding */
			sample = AE_SRAA32RS(sample, 8);
			sample = AE_SLAI32S(sample, 8);
			AE_SA32X2_IP(sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			/* load one 32 bit sample */
			AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
			/* shift with saturation and rounding and store one 32 bit sample */
			sample = AE_SRAA32RS(sample, 8);
			sample = AE_SLAI32S(sample, 8);
			AE_S32_L_IP(sample, (ae_int32 *)out, sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

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
static int pcm_convert_s16_to_f(const struct audio_stream __sparse_cache *source,
				uint32_t ioffset, struct audio_stream __sparse_cache *sink,
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
static int pcm_convert_f_to_s16(const struct audio_stream __sparse_cache *source,
				uint32_t ioffset, struct audio_stream __sparse_cache *sink,
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
static int pcm_convert_s24_to_f(const struct audio_stream __sparse_cache *source,
				uint32_t ioffset, struct audio_stream __sparse_cache *sink,
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
static int pcm_convert_f_to_s24(const struct audio_stream __sparse_cache *source,
				uint32_t ioffset, struct audio_stream __sparse_cache *sink,
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
static int pcm_convert_s32_to_f(const struct audio_stream __sparse_cache *source,
				uint32_t ioffset, struct audio_stream __sparse_cache *sink,
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
static int pcm_convert_f_to_s32(const struct audio_stream __sparse_cache *source,
				uint32_t ioffset, struct audio_stream __sparse_cache *sink,
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
#if CONFIG_PCM_CONVERTER_FORMAT_S24_3LE
	{ SOF_IPC_FRAME_S24_3LE, SOF_IPC_FRAME_S24_3LE, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S24_3LE */
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

#if CONFIG_PCM_CONVERTER_FORMAT_S16_C16_AND_S16_C32
static int pcm_convert_s16_c16_to_s16_c32(const struct audio_stream __sparse_cache *source,
					  uint32_t ioffset,
					  struct audio_stream __sparse_cache *sink,
					  uint32_t ooffset, uint32_t samples)
{
	int16_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int16x4 sample = AE_ZERO16();
	uint32_t nmax, i, n, m, left, left_samples;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();

	ae_int16x4 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s16(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 2;
		left = n & 0x03;

		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load four 16 bit samples */
			AE_LA16X4_IP(sample, inu, in);
			/* shift right and store four 32 bit samples */
			AE_SA32X2_IP(AE_SEXT32X2D16_32(sample), outu, out);
			AE_SA32X2_IP(AE_SEXT32X2D16_10(sample), outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			/* load one 16 bit samples */
			AE_L16_IP(sample, (ae_int16 *)in, sizeof(ae_int16));

			/* store one 32 bit sample */
			AE_S32_L_IP(AE_SEXT32X2D16_32(sample), (ae_int32 *)out, sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}
	return samples;
}

static int pcm_convert_s16_c32_to_s16_c16(const struct audio_stream __sparse_cache *source,
					  uint32_t ioffset,
					  struct audio_stream __sparse_cache *sink,
					  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int16_t *dst = sink->w_ptr;
	ae_int16x4 sample = AE_ZERO16();
	ae_int32x2 sample_1 = AE_ZERO32();
	ae_int32x2 sample_2 = AE_ZERO32();
	uint32_t nmax, i, n, m, left, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int16x4 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(n, nmax);
		m = n >> 2;
		left = n & 0x3;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load four 32 bit samples */
			AE_LA32X2_IP(sample_1, inu, in);
			AE_LA32X2_IP(sample_2, inu, in);

			/* truncate the LSB 16bit of four 32-bit signed elements*/
			sample = AE_CVT16X4(sample_1, sample_2);

			/* store four 16 bit samples */
			AE_SA16X4_IP(sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			/* load one 32 bit samples */
			AE_L32_IP(sample_1, (ae_int32 *)in, sizeof(ae_int32));
			/* shift and round */
			sample = AE_CVT16X4(sample_1, sample_1);
			/* store one 16 bit sample */
			AE_S16_0_IP(AE_MOVAD16_0(sample), (ae_int16 *)out, sizeof(ae_int16));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

	return samples;
}
#endif

#if CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S32_C32
static int pcm_convert_s16_c32_to_s32_c32(const struct audio_stream __sparse_cache *source,
					  uint32_t ioffset,
					  struct audio_stream __sparse_cache *sink,
					  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int32x2 sample = AE_ZERO32();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA32X2_IP(sample, inu, in);
			/* shift left and store two 32 bit samples */
			AE_SA32X2_IP(AE_SLAI32(sample, 16), outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_S32_L_IP(AE_SLAI32(sample, 16), (ae_int32 *)out, sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

	return samples;
}

static int pcm_convert_s32_c32_to_s16_c32(const struct audio_stream __sparse_cache *source,
					  uint32_t ioffset,
					  struct audio_stream __sparse_cache *sink,
					  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int32x2 sample = AE_ZERO32();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA32X2_IP(sample, inu, in);
			/* shift right and store two 32 bit samples */
			AE_SA32X2_IP(AE_SRAA32RS(sample, 16), outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_S32_L_IP(AE_SRAA32RS(sample, 16), (ae_int32 *)out, sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

	return samples;
}
#endif

#if CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S24_C32
static int pcm_convert_s16_c32_to_s24_c32(const struct audio_stream __sparse_cache *source,
					  uint32_t ioffset,
					  struct audio_stream __sparse_cache *sink,
					  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int32x2 sample = AE_ZERO32();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA32X2_IP(sample, inu, in);
			/* shift left and store two 32 bit samples */
			AE_SA32X2_IP(AE_SLAI32(sample, 8), outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_S32_L_IP(AE_SLAI32(sample, 8), (ae_int32 *)out, sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

	return samples;
}

static ae_int32x2 pcm_shift_s24_c32_to_s16(ae_int32x2 sample)
{
	/*get the lsb 24bit,sign_extend_s24 */
	sample = AE_MOVINT24X2_FROMF32X2(sample);
	sample = AE_SLAI32S(sample, 8);
	sample = AE_SRAA32RS(sample, 8);
	/* Q_SHIFT_RND */
	sample = AE_SRAA32RS(sample, 8);

	return sample;
}

static int pcm_convert_s24_c32_to_s16_c32(const struct audio_stream __sparse_cache *source,
					  uint32_t ioffset,
					  struct audio_stream __sparse_cache *sink,
					  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int32x2 sample = AE_ZERO32();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA32X2_IP(sample, inu, in);
			sample = pcm_shift_s24_c32_to_s16(sample);
			/* shift left and store two 32 bit samples */
			AE_SA32X2_IP(sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
			sample = pcm_shift_s24_c32_to_s16(sample);
			AE_S32_L_IP(sample, (ae_int32 *)out, sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

	return samples;
}
#endif

#if CONFIG_PCM_CONVERTER_FORMAT_S24_C24_AND_S24_C32
static int pcm_convert_s24_c24_to_s24_c32(const struct audio_stream __sparse_cache *source,
					  uint32_t ioffset,
					  struct audio_stream __sparse_cache *sink,
					  uint32_t ooffset, uint32_t samples)
{
	uint8_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;
	ae_int24x2 sample24 =  AE_ZERO24();
	ae_int32x2 sample = AE_ZERO32();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int24x2 *in = audio_stream_wrap(source, src + ioffset * 3);
	ae_int32x2 *out = audio_stream_wrap(sink, dst + ooffset);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_bytes_without_wrap(source, in) / 3;
		n = MIN(left_samples, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA24X2_IP(sample24, inu, in);
			sample = AE_MOVINT32_FROMINT24X2(sample24);
			/* shift left and store two 32 bit samples */
			AE_SA32X2_IP(sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			AE_LA24_IP(sample24, inu, in);
			sample = AE_MOVINT32_FROMINT24X2(sample24);
			AE_S32_L_IP(sample, (ae_int32 *)out,
				    sizeof(ae_int32));
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

	return samples;
}

static int pcm_convert_s24_c32_to_s24_c24(const struct audio_stream __sparse_cache *source,
					  uint32_t ioffset,
					  struct audio_stream __sparse_cache *sink,
					  uint32_t ooffset, uint32_t samples)
{
	int32_t *src = source->r_ptr;
	uint8_t *dst = sink->w_ptr;
	ae_int32x2 sample = AE_ZERO32();
	ae_int24x2 sample24 =  AE_ZERO24();
	uint32_t nmax, i, n, m, left_samples;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();

	ae_int32x2 *in = audio_stream_wrap(source, src + ioffset);
	ae_int24x2 *out = audio_stream_wrap(sink, dst + ooffset * 3);

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(left_samples, nmax);
		nmax = audio_stream_bytes_without_wrap(sink, out) / 3;
		n = MIN(n, nmax);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			/* load 2 32 bit samples */
			AE_LA32X2_IP(sample, inu, in);
			sample24 = AE_MOVINT24X2_FROMF32X2(sample);
			AE_SA24X2_IP(sample24, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left 1 sample to avoid memory access overrun */
		if (n & 0x01) {
			AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
			sample24 = AE_MOVINT24X2_FROMF32X2(sample);
			AE_SA24_IP(sample24, outu, out);
		}

		in = audio_stream_wrap(source, in);
		out = audio_stream_wrap(sink, out);
	}

	return samples;
}

/* Haven't implement the pcm_convert_s24_c32_to_s24_c24_link_gtw since there are some
 * errors in that function. Need to wait for the function to be corrected and verified.
 */
#endif

/* Different gateway has different sample layout requirement
 * (1) hda link gateway: 24le sample should be converted to 24be one
 * (2) alh gateway: all data format layout should be in big-endian style in 32bit container,
 *     .e.g. 24le stream should be convert to 24be one
 * (3) ssp gateway: all sample should be in container size of 32bit
 */
const struct pcm_func_vc_map pcm_func_vc_map[] = {
#if CONFIG_PCM_CONVERTER_FORMAT_S16_C16_AND_S16_C32
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,
		ipc4_gtw_all, ipc4_bidirection, pcm_convert_s16_c16_to_s16_c32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,
		ipc4_gtw_all, ipc4_bidirection, pcm_convert_s16_c32_to_s16_c16 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S32_C32
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,
		ipc4_gtw_all, ipc4_bidirection, pcm_convert_s16_c32_to_s32_c32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,
		ipc4_gtw_all, ipc4_bidirection, pcm_convert_s32_c32_to_s16_c32 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S24_C32
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_all & ~ipc4_gtw_alh, ipc4_bidirection, pcm_convert_s16_c32_to_s24_c32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_alh, ipc4_capture, pcm_convert_s32_to_s24 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,
		ipc4_gtw_all & ~ipc4_gtw_alh, ipc4_bidirection, pcm_convert_s24_c32_to_s16_c32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,
		ipc4_gtw_alh, ipc4_playback, pcm_convert_s24_to_s32 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_all & ~(ipc4_gtw_link | ipc4_gtw_alh | ipc4_gtw_host | ipc4_gtw_dmic),
		ipc4_bidirection, audio_stream_copy},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_link | ipc4_gtw_alh, ipc4_playback, pcm_convert_s24_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_link | ipc4_gtw_alh | ipc4_gtw_dmic, ipc4_capture,
		pcm_convert_s32_to_s24 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_host, ipc4_playback, pcm_convert_s32_to_s24 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_host, ipc4_capture, pcm_convert_s24_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,
		ipc4_gtw_all, ipc4_bidirection, pcm_convert_s24_to_s32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_all & ~(ipc4_gtw_link | ipc4_gtw_alh), ipc4_bidirection,
		pcm_convert_s32_to_s24 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_link | ipc4_gtw_alh, ipc4_playback, pcm_convert_s32_to_s24_be },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		ipc4_gtw_link | ipc4_gtw_alh, ipc4_capture, pcm_convert_s32_to_s24 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE, ipc4_gtw_all & ~(ipc4_gtw_link | ipc4_gtw_alh),
		ipc4_bidirection, pcm_convert_s16_to_s24 },
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE, ipc4_gtw_link | ipc4_gtw_alh, ipc4_playback,
		pcm_convert_s16_to_s32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,
		SOF_IPC_FRAME_S16_LE, ipc4_gtw_all, ipc4_bidirection, pcm_convert_s24_to_s16 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S24_C24_AND_S24_C32
	{ SOF_IPC_FRAME_S24_3LE, SOF_IPC_FRAME_S24_3LE, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE, ipc4_gtw_all, ipc4_bidirection,
		pcm_convert_s24_c24_to_s24_c32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_3LE,
		SOF_IPC_FRAME_S24_3LE, ipc4_gtw_all & ~ipc4_gtw_link,
		ipc4_bidirection, pcm_convert_s24_c32_to_s24_c24 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S16_C32
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S16_LE, ipc4_gtw_all, ipc4_bidirection, audio_stream_copy },
#endif
};

const size_t pcm_func_vc_count = ARRAY_SIZE(pcm_func_vc_map);

#endif

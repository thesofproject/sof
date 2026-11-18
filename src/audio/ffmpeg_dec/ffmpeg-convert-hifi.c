// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Xtensa HiFi3 / HiFi4 / HiFi5 VFPU SIMD routines for FFmpeg PCM format
// conversion and interleaving.
//
// Each helper processes the SIMD-aligned prefix of @nb_samples and returns the
// number of samples per channel converted (0 when the target core or compiler
// does not provide the required TIE intrinsics). The caller in
// ffmpeg_dec-convert.c finishes any remaining tail samples in scalar C.

#include <stdint.h>
#include "ffmpeg_dec-convert.h"

#if defined(__XTENSA__) && defined(__has_include)
#if __has_include(<xtensa/config/core-isa.h>)
#include <xtensa/config/core-isa.h>
#endif

#if defined(__has_builtin) && __has_builtin(__builtin_xtensa_trunc_sx2) && \
	__has_builtin(__builtin_xtensa_float_sx2) && \
	defined(XCHAL_HAVE_HIFI4_VFPU) && XCHAL_HAVE_HIFI4_VFPU && \
	__has_include(<xtensahifiintrin.h>)
#include <xtensahifiintrin.h>
#define FFMPEG_DEC_CONV_VFPU 1
#endif

#if defined(XCHAL_HAVE_HIFI3) && XCHAL_HAVE_HIFI3 && __has_include(<xtensa/tie/xt_hifi3.h>)
#include <xtensa/tie/xt_hifi3.h>
#define FFMPEG_DEC_CONV_HIFI3 1
#endif
#endif

int ffmpeg_hifi_fltp_to_s32(const float *const *src, int32_t *dst,
			    int channels, int nb_samples)
{
#if defined(FFMPEG_DEC_CONV_VFPU)
	int i = 0;

	if (channels == 1) {
		const float *c0 = src[0];

		for (; i + 2 <= nb_samples; i += 2) {
			int32_t tmp[2] __attribute__((aligned(8)));
			ae_int32x2 f = AE_L32X2_I((const ae_int32x2 *)&c0[i], 0);
			ae_int32x2 q = XT_TRUNC_SX2((ae_xtfloatx2)f, 31);

			AE_S32X2_I(q, (ae_int32x2 *)tmp, 0);
			dst[i + 0] = tmp[0];
			dst[i + 1] = tmp[1];
		}
		return i;
	}

	if (channels == 2) {
		const float *c0 = src[0];
		const float *c1 = src[1];

		for (; i + 2 <= nb_samples; i += 2) {
			ae_int32x2 f0 = AE_L32X2_I((const ae_int32x2 *)&c0[i], 0);
			ae_int32x2 f1 = AE_L32X2_I((const ae_int32x2 *)&c1[i], 0);
			ae_int32x2 q0 = XT_TRUNC_SX2((ae_xtfloatx2)f0, 31);
			ae_int32x2 q1 = XT_TRUNC_SX2((ae_xtfloatx2)f1, 31);
			int32_t tmp0[2] __attribute__((aligned(8)));
			int32_t tmp1[2] __attribute__((aligned(8)));

			AE_S32X2_I(q0, tmp0, 0);
			AE_S32X2_I(q1, tmp1, 0);

			dst[2 * i + 0] = tmp0[0];
			dst[2 * i + 1] = tmp1[0];
			dst[2 * i + 2] = tmp0[1];
			dst[2 * i + 3] = tmp1[1];
		}
		return i;
	}

	for (int c = 0; c < channels; c++) {
		const float *pl = src[c];
		int32_t *dc = dst + c;

		for (i = 0; i + 2 <= nb_samples; i += 2) {
			int32_t tmp[2] __attribute__((aligned(8)));
			ae_int32x2 f = AE_L32X2_I((const ae_int32x2 *)&pl[i], 0);
			ae_int32x2 q = XT_TRUNC_SX2((ae_xtfloatx2)f, 31);

			AE_S32X2_I(q, (ae_int32x2 *)tmp, 0);
			dc[i * channels] = tmp[0];
			dc[(i + 1) * channels] = tmp[1];
		}
	}
	return nb_samples & ~1;
#else
	(void)src;
	(void)dst;
	(void)channels;
	(void)nb_samples;
	return 0;
#endif
}

int ffmpeg_hifi_s32_to_fltp(const int32_t *src, float *const *dst,
			    int channels, int nb_samples)
{
#if defined(FFMPEG_DEC_CONV_VFPU)
	int i = 0;

	for (int c = 0; c < channels; c++) {
		float *pl = dst[c];
		const int32_t *sc = src + c;

		for (i = 0; i + 2 <= nb_samples; i += 2) {
			int32_t tmp[2] __attribute__((aligned(8)));
			ae_int32x2 p;
			ae_xtfloatx2 r;

			tmp[0] = sc[i * channels];
			tmp[1] = sc[(i + 1) * channels];
			p = AE_L32X2_I((const ae_int32x2 *)tmp, 0);
			r = XT_FLOAT_SX2(p, 31);
			AE_S32X2_I((ae_int32x2)r, (ae_int32x2 *)&pl[i], 0);
		}
	}
	return nb_samples & ~1;
#else
	(void)src;
	(void)dst;
	(void)channels;
	(void)nb_samples;
	return 0;
#endif
}

int ffmpeg_hifi_s32p_to_s32_stereo(const int32_t *c0, const int32_t *c1,
				   int32_t *dst, int nb_samples)
{
#if defined(FFMPEG_DEC_CONV_HIFI3)
	int i = 0;

	for (; i + 2 <= nb_samples; i += 2) {
		ae_int32x2 v0 = AE_L32X2_I((const ae_int32x2 *)&c0[i], 0);
		ae_int32x2 v1 = AE_L32X2_I((const ae_int32x2 *)&c1[i], 0);
		ae_int32x2 pair0 = AE_SEL32_LL(v0, v1);
		ae_int32x2 pair1 = AE_SEL32_HH(v0, v1);

		AE_S32X2_I(pair0, (ae_int32x2 *)&dst[2 * i + 0], 0);
		AE_S32X2_I(pair1, (ae_int32x2 *)&dst[2 * i + 2], 0);
	}
	return i;
#else
	(void)c0;
	(void)c1;
	(void)dst;
	(void)nb_samples;
	return 0;
#endif
}

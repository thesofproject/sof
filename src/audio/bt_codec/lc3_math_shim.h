/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Self-contained IEEE-754 single-precision math helpers for liblc3 under
 * Zephyr CONFIG_MINIMAL_LIBC (no external libm link dependency required).
 */

#ifndef __SOF_AUDIO_BT_CODEC_LC3_MATH_SHIM_H__
#define __SOF_AUDIO_BT_CODEC_LC3_MATH_SHIM_H__

#include <stdint.h>
#include <math.h>

#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif

#ifndef NAN
#define NAN (__builtin_nanf(""))
#endif

static inline float sof_lc3_fabsf(float x)
{
	return __builtin_fabsf(x);
}

static inline float sof_lc3_fminf(float a, float b)
{
	return a < b ? a : b;
}

static inline float sof_lc3_fmaxf(float a, float b)
{
	return a > b ? a : b;
}

static inline float sof_lc3_truncf(float x)
{
	union {
		float f;
		uint32_t u;
	} v = { .f = x };

	/* For |x| >= 2^23 (biased exponent >= 150), x has no fractional bits */
	if (((v.u >> 23) & 0xFFu) >= 150u)
		return x;

	return (float)(int32_t)x;
}

static inline float sof_lc3_floorf(float x)
{
	float t = sof_lc3_truncf(x);

	return t - (x < t ? 1.0f : 0.0f);
}

static inline float sof_lc3_ceilf(float x)
{
	float t = sof_lc3_truncf(x);

	return t + (x > t ? 1.0f : 0.0f);
}

static inline float sof_lc3_roundf(float x)
{
	union {
		float f;
		uint32_t u;
	} v = { .f = x };

	if (((v.u >> 23) & 0xFFu) >= 150u)
		return x;

	return x >= 0.0f ? sof_lc3_truncf(x + 0.5f) : sof_lc3_truncf(x - 0.5f);
}

static inline float sof_lc3_ldexpf(float x, int exp)
{
	union {
		float f;
		uint32_t u;
	} v = { .f = x };
	int e;

	if (x == 0.0f)
		return 0.0f;
	e = (int)((v.u >> 23) & 0xFF) + exp;
	if (e <= 0)
		return 0.0f;
	if (e >= 255)
		e = 254;
	v.u = (v.u & 0x807FFFFFu) | ((uint32_t)e << 23);
	return v.f;
}

static inline float sof_lc3_frexpf(float x, int *exp)
{
	union {
		float f;
		uint32_t u;
	} v = { .f = x };
	int e;

	if (x == 0.0f) {
		*exp = 0;
		return 0.0f;
	}
	e = (int)((v.u >> 23) & 0xFF);
	if (e == 0) {
		*exp = 0;
		return 0.0f;
	}
	*exp = e - 126;
	v.u = (v.u & 0x807FFFFFu) | (126u << 23);
	return v.f;
}

static inline float sof_lc3_sqrtf(float x)
{
	union {
		float f;
		uint32_t u;
	} v;
	float y;

	if (x <= 0.0f)
		return 0.0f;
	v.f = x;
	v.u = 0x5f3759dfu - (v.u >> 1);
	y = v.f;
	y = y * (1.5f - 0.5f * x * y * y);
	y = y * (1.5f - 0.5f * x * y * y);
	y = y * (1.5f - 0.5f * x * y * y);
	return x * y;
}

static inline float sof_lc3_log10f(float x)
{
	int e = 0;
	float m, y;

	if (x <= 0.0f)
		return -38.0f;
	m = sof_lc3_frexpf(x, &e);
	y = (((-1.29479677f * m + 5.11769018f) * m - 8.42295281f) * m +
	     8.10557963f) * m - 3.50567360f;
	return 0.30102999566f * ((float)e + y);
}

#undef fabsf
#undef fminf
#undef fmaxf
#undef truncf
#undef floorf
#undef ceilf
#undef roundf
#undef ldexpf
#undef frexpf
#undef sqrtf
#undef log10f

#define fabsf  sof_lc3_fabsf
#define fminf  sof_lc3_fminf
#define fmaxf  sof_lc3_fmaxf
#define truncf sof_lc3_truncf
#define floorf sof_lc3_floorf
#define ceilf  sof_lc3_ceilf
#define roundf sof_lc3_roundf
#define ldexpf sof_lc3_ldexpf
#define frexpf sof_lc3_frexpf
#define sqrtf  sof_lc3_sqrtf
#define log10f sof_lc3_log10f

#endif /* __SOF_AUDIO_BT_CODEC_LC3_MATH_SHIM_H__ */

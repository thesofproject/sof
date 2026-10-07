/* SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
 * Copyright (c) 2026 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Steam Audio DSP Embedded Math Library
 */

#ifndef __SOF_AUDIO_STEAMAUDIO_MATH_H__
#define __SOF_AUDIO_STEAMAUDIO_MATH_H__

#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

static inline float sat_clamp(float val, float min_v, float max_v)
{
	if (val < min_v) return min_v;
	if (val > max_v) return max_v;
	return val;
}

static inline float fast_fabsf(float x)
{
	return __builtin_fabsf(x);
}

static inline float fast_fminf(float a, float b)
{
	return (a < b) ? a : b;
}

static inline float fast_fmaxf(float a, float b)
{
	return (a > b) ? a : b;
}

static inline float fast_floorf(float x)
{
	int i = (int)x;
	return (x < 0.0f && x != (float)i) ? (float)(i - 1) : (float)i;
}

static inline float fast_inv_sqrt(float x)
{
	if (x <= 0.0f) return 0.0f;
	union { float f; uint32_t i; } conv = { .f = x };
	conv.i = 0x5f3759df - (conv.i >> 1);
	conv.f *= (1.5f - (0.5f * x * conv.f * conv.f));
	return conv.f;
}

static inline float fast_sqrt(float x)
{
	if (x <= 0.0f) return 0.0f;
	return __builtin_sqrtf(x);
}

static inline float fast_sin(float x)
{
	while (x < -PI) x += TWO_PI;
	while (x > PI) x -= TWO_PI;
	if (x > 0.5f * PI)
		x = PI - x;
	else if (x < -0.5f * PI)
		x = -PI - x;
	float x2 = x * x;
	return x * (1.0f - x2 * (0.16666667f - x2 * (0.00833333f - x2 * 0.00019841f)));
}

static inline float fast_cos(float x)
{
	return fast_sin(x + 0.5f * PI);
}

static inline float fast_expf(float x)
{
	if (x < -16.0f) return 0.0f;
	if (x > 16.0f) x = 16.0f;
	float p = 1.0f + x * (1.0f + x * (0.5f + x * (0.16666667f + x * (0.04166667f + x * 0.00833333f))));
	return (p > 0.0f) ? p : 0.0f;
}

static inline float fast_logf(float x)
{
	if (x <= 0.0f) return -80.0f;
	union { float f; uint32_t i; } u = { .f = x };
	int e = ((u.i >> 23) & 0xff) - 127;
	u.i = (u.i & 0x007fffff) | 0x3f800000;
	float m = u.f;
	float y = (m - 1.0f) / (m + 1.0f);
	float y2 = y * y;
	float res = 2.0f * y * (1.0f + y2 * (0.33333333f + y2 * 0.2f));
	return res + (float)e * 0.69314718f;
}

static inline float fast_log10f(float x)
{
	return fast_logf(x) * 0.43429448f;
}

static inline float fast_powf(float base, float exp)
{
	if (base <= 0.0f) return 0.0f;
	if (exp == 0.0f) return 1.0f;
	if (exp == 1.0f) return base;
	if (exp == 2.0f) return base * base;
	if (exp == 0.5f) return fast_sqrt(base);
	return fast_expf(exp * fast_logf(base));
}

static inline float fast_atan2f(float y, float x)
{
	if (x == 0.0f) return (y > 0.0f) ? (0.5f * PI) : ((y < 0.0f) ? (-0.5f * PI) : 0.0f);
	float abs_y = fast_fabsf(y) + 1e-10f;
	float angle;
	if (x >= 0.0f) {
		float r = (x - abs_y) / (x + abs_y);
		angle = 0.25f * PI - 0.25f * PI * r;
	} else {
		float r = (x + abs_y) / (abs_y - x);
		angle = 0.75f * PI - 0.25f * PI * r;
	}
	return (y < 0.0f) ? -angle : angle;
}

static inline float fast_asinf(float x)
{
	if (x <= -1.0f) return -0.5f * PI;
	if (x >= 1.0f) return 0.5f * PI;
	return fast_atan2f(x, fast_sqrt(1.0f - x * x));
}

static inline float fast_acosf(float x)
{
	return 0.5f * PI - fast_asinf(x);
}

static inline float fast_atan2(float y, float x)
{
	return fast_atan2f(y, x);
}

static inline float fast_acos(float x)
{
	return fast_acosf(x);
}

#undef fabsf
#define fabsf fast_fabsf
#undef fminf
#define fminf fast_fminf
#undef fmaxf
#define fmaxf fast_fmaxf
#undef floorf
#define floorf fast_floorf
#undef sqrtf
#define sqrtf fast_sqrt
#undef sinf
#define sinf fast_sin
#undef cosf
#define cosf fast_cos
#undef expf
#define expf fast_expf
#undef log10f
#define log10f fast_log10f
#undef powf
#define powf fast_powf
#undef atan2f
#define atan2f fast_atan2f
#undef asinf
#define asinf fast_asinf
#undef acosf
#define acosf fast_acosf

#endif /* __SOF_AUDIO_STEAMAUDIO_MATH_H__ */

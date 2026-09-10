// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// High-performance self-contained math library for WebRTC Float AEC on Xtensa DSP.
// Optimized for soft-float execution by eliminating divisions, using fast bitwise
// IEEE-754 approximations and lookup tables.

#include <stdint.h>
#include <stdbool.h>

/* --- helpers --- */

static inline float aec_ldexp2(int n)
{
	union { float f; uint32_t i; } u;

	if (n < -126) return 0.0f;
	if (n > 127)  n = 127;
	u.i = (uint32_t)(n + 127) << 23;
	return u.f;
}



/* --- fast log2f: bit conversion + degree-3 polynomial without division --- */
static float aec_log2f(float x)
{
	union { float f; uint32_t i; } u = { .f = x };
	int e;
	float m, z;

	if (x <= 0.0f)
		return -127.0f;

	e = (int)((u.i >> 23) & 0xff) - 127;
	u.i = (u.i & 0x007fffff) | 0x3f800000;
	m = u.f;

	z = m - 1.0f;
	return (float)e + z * (1.44269504f - z * (0.72134752f - z * 0.48089834f));
}

/* --- fast exp2f: split x = n + f, degree-3 polynomial --- */
float exp2f(float x)
{
	int n;
	float f, p;

	if (x > 127.0f)
		return 3.40282347e+38f;
	if (x < -126.0f)
		return 0.0f;

	n = (int)x;
	if (x < 0.0f && (float)n != x)
		n--;
	f = x - (float)n;

	p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * 0.0555041f));
	return p * aec_ldexp2(n);
}


/* --- powf: x^y = 2^(y * log2(x)) --- */
float powf(float x, float y)
{
	if (x <= 0.0f)
		return (x == 0.0f && y > 0.0f) ? 0.0f : 1.0f;
	if (x >= 0.9999f && x <= 1.0001f)
		return 1.0f;
	if (y == 0.0f)
		return 1.0f;
	if (y == 1.0f)
		return x;
	if (y == 2.0f)
		return x * x;
	if (y == 0.5f)
		return __builtin_sqrtf(x);
	return exp2f((float)(y * aec_log2f(x)));
}

double pow(double x, double y)
{
	return (double)powf((float)x, (float)y);
}

/* --- logf and log10f --- */
float logf(float x)
{
	return aec_log2f(x) * 0.6931471805599453f;
}

double log(double x)
{
	return (double)logf((float)x);
}

float log10f(float x)
{
	return aec_log2f(x) * 0.3010299956639812f;
}

double log10(double x)
{
	return (double)log10f((float)x);
}

/* --- fast sinf/cosf: 16-point table for comfort noise and phase generation --- */
static const float sin_table_16[16] = {
	 0.00000000f,  0.38268343f,  0.70710678f,  0.92387953f,
	 1.00000000f,  0.92387953f,  0.70710678f,  0.38268343f,
	 0.00000000f, -0.38268343f, -0.70710678f, -0.92387953f,
	-1.00000000f, -0.92387953f, -0.70710678f, -0.38268343f
};

float sinf(float x)
{
	/* Normalize x in [0, 2pi) to [0, 16) */
	const float rad_to_idx = 2.54647909f; /* 16 / (2 * pi) */
	int idx = (int)(x * rad_to_idx) % 16;
	if (idx < 0)
		idx += 16;
	return sin_table_16[idx];
}

double sin(double x)
{
	return (double)sinf((float)x);
}

float cosf(float x)
{
	const float rad_to_idx = 2.54647909f;
	int idx = ((int)(x * rad_to_idx) + 4) % 16;
	if (idx < 0)
		idx += 16;
	return sin_table_16[idx];
}

double cos(double x)
{
	return (double)cosf((float)x);
}

/* --- floor / ceil / round --- */
float floorf(float x)
{
	int i = (int)x;
	if (x < 0.0f && (float)i != x)
		i--;
	return (float)i;
}

double floor(double x)
{
	return (double)floorf((float)x);
}

float ceilf(float x)
{
	int i = (int)x;
	if (x > 0.0f && (float)i != x)
		i++;
	return (float)i;
}

double ceil(double x)
{
	return (double)ceilf((float)x);
}

float roundf(float x)
{
	return (x >= 0.0f) ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}

double round(double x)
{
	return (double)roundf((float)x);
}

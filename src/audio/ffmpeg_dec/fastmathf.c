// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Fast single-precision float math for FFmpeg lossy decoders (AAC-LC / Opus).
//
// Empirically these decoders need only 7 single-precision functions on their
// decode paths: powf, sqrtf, cbrtf, exp2f, log2f, sinf, cosf. SOF's own math is
// fixed-point Q-format (range-limited) and cannot serve them, so this provides
// IEEE single-precision approximations. 24-bit mantissa precision is ample for
// 32-bit audio samples destined for 24/16-bit rendering, and these are per-frame
// / per-band calls (not the innermost per-sample loop, which is MDCT + table
// lookups), so scalar polynomial/Newton approximations are fast enough.
//
// On Xtensa HiFi, sqrtf/rsqrt and FMA map to hardware intrinsics; the scalar C
// here is the portable reference. Marked spots are where HiFi intrinsics would
// slot in.
//
// Build standalone accuracy/perf test:
//   gcc -O2 -DSOFM_FASTMATH_TEST fastmathf.c -lm -o t && ./t

#include <stdint.h>

/* --- helpers --- */

static inline float sofm_flint(int n)
{
	return (float)n;
}

/* Build 2^n as a float via the exponent field (n in [-126,127]). */
static inline float sofm_ldexp2(int n)
{
	union { float f; uint32_t i; } u;

	u.i = (uint32_t)(n + 127) << 23;
	return u.f;
}

/* --- sqrtf: bit-trick seed + 2 Newton steps --- */
float sofm_sqrtf(float x)
{
	union { float f; uint32_t i; } u = { .f = x };
	float y;

	if (x <= 0.0f)
		return 0.0f;

	/* HiFi: use FSQRT/RSQRT intrinsic instead of this seed. */
	u.i = 0x1fbd1df5 + (u.i >> 1);
	y = u.f;
	y = 0.5f * (y + x / y);
	y = 0.5f * (y + x / y);
	return y;
}

/* --- cbrtf: bit-trick seed + 2 Newton steps, sign-symmetric --- */
float sofm_cbrtf(float x)
{
	union { float f; uint32_t i; } u;
	int neg = x < 0.0f;
	float a = neg ? -x : x;
	float y;

	if (x == 0.0f)
		return 0.0f;

	u.f = a;
	u.i = u.i / 3 + 0x2a514067;
	y = u.f;
	y = (2.0f * y + a / (y * y)) * (1.0f / 3.0f);
	y = (2.0f * y + a / (y * y)) * (1.0f / 3.0f);
	return neg ? -y : y;
}

/* --- exp2f: split x = n + f, 2^x = 2^n * 2^f, poly for 2^f on [0,1) --- */
float sofm_exp2f(float x)
{
	int n;
	float f, p;

	if (x > 127.0f)
		return sofm_ldexp2(127) * 2.0f;   /* +inf-ish */
	if (x < -126.0f)
		return 0.0f;

	n = (int)x;
	if (x < 0.0f && (float)n != x)
		n--;                              /* floor */
	f = x - (float)n;

	/* Taylor of 2^f, coeffs (ln2)^k/k!, degree 5 -> ~1e-6 rel error. */
	p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f +
	    f * (0.0096181f + f * 0.0013334f))));
	return p * sofm_ldexp2(n);
}

/* --- log2f: x = 2^e * m, reduce m to [sqrt(2)/2, sqrt(2)), atanh series --- */
float sofm_log2f(float x)
{
	union { float f; uint32_t i; } u = { .f = x };
	int e;
	float m, a, a2, ln_m;

	if (x <= 0.0f)
		return -127.0f;

	e = (int)((u.i >> 23) & 0xff) - 127;
	u.i = (u.i & 0x007fffff) | 0x3f800000;   /* m in [1,2) */
	m = u.f;
	if (m > 1.41421356f) {                    /* center around 1: m in [~.707,~1.414) */
		m *= 0.5f;
		e += 1;
	}

	/* ln(m) = 2*atanh(a), a = (m-1)/(m+1); exact 0 at m==1. */
	a = (m - 1.0f) / (m + 1.0f);
	a2 = a * a;
	ln_m = 2.0f * a * (1.0f + a2 * (0.3333333f + a2 * (0.2f + a2 * 0.14285714f)));
	return (float)e + ln_m * 1.44269504f;     /* / ln(2) */
}

/* --- powf: x^y = 2^(y*log2(x)) for x > 0 (AAC uses non-negative magnitudes) --- */
float sofm_powf(float x, float y)
{
	if (x <= 0.0f)
		return 0.0f;
	return sofm_exp2f(y * sofm_log2f(x));
}

/* --- sinf/cosf: Cody-Waite reduction by pi/2 + quadrant, polys on [-pi/4,pi/4] --- */
static float sofm_poly_sin(float r)
{
	float r2 = r * r;

	return r * (1.0f + r2 * (-0.16666667f + r2 * (0.00833216f +
		  r2 * -0.00019515f)));
}

static float sofm_poly_cos(float r)
{
	float r2 = r * r;

	return 1.0f + r2 * (-0.5f + r2 * (0.04166664f +
	       r2 * (-0.00138873f + r2 * 2.4433e-5f)));
}

static float sofm_sincos_core(float x, int cos_phase)
{
	/* pi/2 split so k*PIO2_hi is exact for moderate k (Cody-Waite). */
	const float two_over_pi = 0.636619772f;
	const float PIO2_hi = 1.5707855225f;
	const float PIO2_lo = 1.08043006e-5f;
	int k = (int)(x * two_over_pi + (x < 0.0f ? -0.5f : 0.5f));
	float r = (x - (float)k * PIO2_hi) - (float)k * PIO2_lo;
	int q = (k + cos_phase) & 3;

	switch (q) {
	case 0:  return sofm_poly_sin(r);
	case 1:  return sofm_poly_cos(r);
	case 2:  return -sofm_poly_sin(r);
	default: return -sofm_poly_cos(r);
	}
}

float sofm_sinf(float x)
{
	return sofm_sincos_core(x, 0);
}

float sofm_cosf(float x)
{
	return sofm_sincos_core(x, 1);
}

/*
 * libc-name entry points used by libavcodec. Defined here so the LLEXT resolves
 * them internally. (In a native/testbench build the real libm provides these;
 * this file, like the other shims, is LLEXT-only.)
 */
#ifndef SOFM_FASTMATH_TEST
float sqrtf(float x)         { return sofm_sqrtf(x); }
float cbrtf(float x)         { return sofm_cbrtf(x); }
float exp2f(float x)         { return sofm_exp2f(x); }
float log2f(float x)         { return sofm_log2f(x); }
float powf(float x, float y) { return sofm_powf(x, y); }
float sinf(float x)          { return sofm_sinf(x); }
float cosf(float x)          { return sofm_cosf(x); }
#endif

#ifdef SOFM_FASTMATH_TEST
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

/* metric: 0 = relative error, 1 = absolute error */
static double maxerr(const char *name, float (*approx)(float),
		     double (*ref)(double), float lo, float hi, int log_sweep,
		     int absolute)
{
	double worst = 0.0, at = 0.0;
	int i, N = 200000;

	for (i = 0; i < N; i++) {
		double t = (double)i / (N - 1);
		float x = log_sweep ? (float)(lo * pow(hi / lo, t))
				    : (float)(lo + (hi - lo) * t);
		double r = ref((double)x), a = approx(x);
		double e = absolute ? fabs(a - r)
			 : (fabs(r) > 1e-9 ? fabs((a - r) / r) : fabs(a - r));

		if (e > worst) { worst = e; at = x; }
	}
	printf("  %-8s max_%s_err=%.3e at x=%.6g\n", name,
	       absolute ? "abs" : "rel", worst, at);
	return worst;
}

static double refpow_x(double x) { return pow(x, 1.3333333333); }

int main(void)
{
	volatile float acc = 0;
	int i;
	clock_t t0;

	printf("accuracy (approx vs libm double reference):\n");
	maxerr("sqrtf", sofm_sqrtf, sqrt, 1e-6f, 1e6f, 1, 0);
	maxerr("cbrtf", sofm_cbrtf, cbrt, 1e-6f, 1e6f, 1, 0);
	maxerr("exp2f", sofm_exp2f, exp2, -30.f, 30.f, 0, 0);
	maxerr("log2f", sofm_log2f, log2, 1e-6f, 1e6f, 1, 1);   /* abs: log2 crosses 0 */
	maxerr("sinf",  sofm_sinf,  sin,  -25.f, 25.f, 0, 1);   /* abs: sin/cos in [-1,1] */
	maxerr("cosf",  sofm_cosf,  cos,  -25.f, 25.f, 0, 1);
	/* powf specifically over the AAC dequant shape x^(4/3), x in [0,8192] */
	{
		double worst = 0, at = 0;
		for (i = 1; i < 200000; i++) {
			float x = (float)(8192.0 * i / 200000.0);
			double r = refpow_x(x), a = sofm_powf(x, 1.3333333f);
			double e = fabs(r) > 1e-9 ? fabs((a - r) / r) : fabs(a - r);
			if (e > worst) { worst = e; at = x; }
		}
		printf("  %-8s max_rel_err=%.3e at x=%.6g (x^4/3)\n", "powf", worst, at);
	}

	/* rough timing: fast vs libm for exp2f/log2f/powf */
	printf("timing (200M calls):\n");
	t0 = clock();
	for (i = 0; i < 200000000; i++) acc += sofm_exp2f((float)(i & 63) - 32.f);
	printf("  sofm_exp2f: %.2fs\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
	t0 = clock();
	for (i = 0; i < 200000000; i++) acc += exp2f((float)(i & 63) - 32.f);
	printf("  libm exp2f: %.2fs\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
	return (int)acc & 0;
}
#endif

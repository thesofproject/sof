// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// libc shims for the webrtc_ns real backend in LLEXT context.
//
// The WebRTC NS C library references malloc/free and a small set of math
// symbols. These are not exported by SOF core to LLEXT modules, so we
// provide thin wrappers backed by SOF's rballoc heap and sofm_ math.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/alloc.h>
#if !defined(LLEXT_MODULE) && !defined(LL_EXTENSION_BUILD)
#include <rtos/userspace_helper.h>
static APP_SYSUSER_BSS struct processing_module *active_ns_mod;
#else
static struct processing_module *active_ns_mod;
#endif

void webrtc_ns_set_active_mod(struct processing_module *mod)
{
	active_ns_mod = mod;
}

void *webrtc_ns_malloc(size_t size)
{
	if (size == 0)
		return NULL;
	if (active_ns_mod)
		return mod_alloc(active_ns_mod, size);
	return rballoc(SOF_MEM_FLAG_USER, size);
}

void webrtc_ns_free(void *ptr)
{
	if (!ptr)
		return;
	if (active_ns_mod) {
		mod_free(active_ns_mod, ptr);
		return;
	}
	rfree(ptr);
}

void *webrtc_ns_calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;

	if (total == 0)
		return NULL;
	if (active_ns_mod)
		return mod_zalloc(active_ns_mod, total);
	void *p = rballoc(SOF_MEM_FLAG_USER, total);

	if (p)
		memset(p, 0, total);
	return p;
}

#if defined(LLEXT_MODULE) || defined(LL_EXTENSION_BUILD)
/* ============================ Memory ============================ */

void *malloc(size_t size)
{
	return webrtc_ns_malloc(size);
}

void free(void *ptr)
{
	webrtc_ns_free(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
	return webrtc_ns_calloc(nmemb, size);
}

/* ============================ String helpers ===================== */

void *memset(void *s, int c, size_t n)
{
	uint8_t *p = s;

	while (n--)
		*p++ = (uint8_t)c;
	return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	const uint8_t *s = src;
	uint8_t *d = dest;

	while (n--)
		*d++ = *s++;
	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	uint8_t *d = dest;
	const uint8_t *s = src;

	if (d < s || d >= s + n) {
		while (n--)
			*d++ = *s++;
	} else {
		d += n;
		s += n;
		while (n--)
			*--d = *--s;
	}
	return dest;
}
#endif

/* ============================ Math ============================== */
/*
 * The NS spectral processing uses log(), exp(), sqrt(), pow(), fabsf().
 * Route to soft-float versions provided here for environments without libm.
 */


__attribute__((weak)) float fabsf(float x)
{
	return (x < 0.0f) ? -x : x;
}

__attribute__((weak)) double fabs(double x)
{
	return (x < 0.0) ? -x : x;
}

static inline float ns_exp2f(float x)
{
	union { float f; uint32_t i; } u;
	float f, p;
	int n;

	if (x >= 127.0f)
		return 3.40282347e+38f;
	if (x < -126.0f)
		return 0.0f;

	n = (int)(x >= 0.0f ? x + 0.5f : x - 0.5f);
	if (n > 127)
		return 3.40282347e+38f;
	if (n < -126)
		return 0.0f;

	f = (x - (float)n) * 0.69314718f;
	p = 1.0f + f * (1.0f + f * (0.5f + f * (0.16666667f +
	    f * (0.04166667f + f * (0.00833333f + f * 0.00138889f)))));
	u.i = (uint32_t)(n + 127) << 23;
	return p * u.f;
}

__attribute__((weak)) float expf(float x)
{
	return ns_exp2f(x * 1.4426950408889634f);
}

__attribute__((weak)) double exp(double x)
{
	return (double)expf((float)x);
}

__attribute__((weak)) double tanh(double x)
{
	if (x > 10.0) return 1.0;
	if (x < -10.0) return -1.0;
	double e2 = exp(2.0 * x);
	return (e2 - 1.0) / (e2 + 1.0);
}

__attribute__((weak)) float atanf(float x)
{
	float ax = fabsf(x);
	float base = 0.0f;
	float u, u2, res;

	if (ax > 2.41421356f) {
		base = 1.5707963267948966f;
		u = -1.0f / ax;
	} else if (ax > 0.41421356f) {
		base = 0.7853981633974483f;
		u = (ax - 1.0f) / (ax + 1.0f);
	} else {
		u = ax;
	}

	u2 = u * u;
	res = base + u * (1.0f + u2 * (-0.33333333f + u2 * (0.20000000f +
	      u2 * (-0.14285714f + u2 * 0.11111111f))));
	return (x < 0.0f) ? -res : res;
}

static float ns_log2f(float x)
{
	union { float f; uint32_t i; } u = { .f = x };
	int e, e_bias = 127;
	float m, r, s, poly;

	if (x <= 0.0f)
		return -127.0f;

	if (u.i < 0x00800000U) {
		x *= 16777216.0f;
		u.f = x;
		e_bias = 151;
	}

	e = (int)((u.i >> 23) & 0xff) - e_bias;
	u.i = (u.i & 0x007fffff) | 0x3f800000;
	m = u.f;
	if (m > 1.4142135623730951f) {
		m *= 0.5f;
		e++;
	}

	r = (m - 1.0f) / (m + 1.0f);
	s = r * r;
	poly = 1.0f + s * (0.33333333f + s * (0.20000000f +
	       s * (0.14285714f + s * 0.11111111f)));

	return (float)e + (2.8853900817779268f * r) * poly;
}

__attribute__((weak)) float logf(float x)
{
	return ns_log2f(x) * 0.6931471805599453f;
}

__attribute__((weak)) double log(double x)
{
	return (double)logf((float)x);
}

__attribute__((weak)) float sqrtf(float x)
{
	if (x <= 0.0f)
		return 0.0f;
	return __builtin_sqrtf(x);
}

__attribute__((weak)) double sqrt(double x)
{
	return (double)sqrtf((float)x);
}

__attribute__((weak)) float powf(float x, float y)
{
	if (x <= 0.0f)
		return (x == 0.0f && y > 0.0f) ? 0.0f : 1.0f;
	if (y == 0.0f)
		return 1.0f;
	if (y == 1.0f)
		return x;
	if (y == 2.0f)
		return x * x;
	if (y == 0.5f)
		return sqrtf(x);
	return ns_exp2f(y * ns_log2f(x));
}

static inline float ns_floorf(float x)
{
	int i = (int)x;

	if (x < 0.0f && (float)i != x)
		i--;
	return (float)i;
}

__attribute__((weak)) float floorf(float x)
{
	return ns_floorf(x);
}

__attribute__((weak)) double floor(double x)
{
	return (double)ns_floorf((float)x);
}

static inline float ns_ceilf(float x)
{
	int i = (int)x;

	if (x > 0.0f && (float)i != x)
		i++;
	return (float)i;
}

static inline float ns_roundf(float x)
{
	return (x >= 0.0f) ? ns_floorf(x + 0.5f) : ns_ceilf(x - 0.5f);
}

static inline void ns_sincosf_poly(float z, float *s_out, float *c_out)
{
	float z2 = z * z;

	*s_out = z * (1.0f + z2 * (-0.166666667f + z2 * (0.008333333f - z2 * 0.0001984127f)));
	*c_out = 1.0f + z2 * (-0.5f + z2 * (0.041666667f + z2 * (-0.0013888889f + z2 * 0.0000248016f)));
}

__attribute__((weak)) float sinf(float x)
{
	const float inv_half_pi = 0.636619772367581343f;
	const float half_pi_hi  = 1.5707963267948966f;
	int k = (int)ns_roundf(x * inv_half_pi);
	float z = x - (float)k * half_pi_hi;
	float s, c;

	ns_sincosf_poly(z, &s, &c);

	switch ((k % 4 + 4) % 4) {
	case 0: return s;
	case 1: return c;
	case 2: return -s;
	case 3: default: return -c;
	}
}

__attribute__((weak)) double sin(double x)
{
	return (double)sinf((float)x);
}

__attribute__((weak)) float cosf(float x)
{
	const float inv_half_pi = 0.636619772367581343f;
	const float half_pi_hi  = 1.5707963267948966f;
	int k = (int)ns_roundf(x * inv_half_pi);
	float z = x - (float)k * half_pi_hi;
	float s, c;

	ns_sincosf_poly(z, &s, &c);

	switch ((k % 4 + 4) % 4) {
	case 0: return c;
	case 1: return -s;
	case 2: return -c;
	case 3: default: return s;
	}
}

__attribute__((weak)) double cos(double x)
{
	return (double)cosf((float)x);
}

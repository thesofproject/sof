// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Memory allocator and math shims for the webrtc_ns2 (RNNoise) backend.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/alloc.h>
#if !defined(LLEXT_MODULE)
#include <rtos/userspace_helper.h>
static APP_SYSUSER_BSS struct processing_module *active_ns2_mod;
#else
static struct processing_module *active_ns2_mod;
#endif

void webrtc_ns2_set_active_mod(struct processing_module *mod)
{
	active_ns2_mod = mod;
}

void *webrtc_ns2_malloc(size_t size)
{
	if (size == 0)
		return NULL;
	if (active_ns2_mod)
		return mod_alloc(active_ns2_mod, size);
	return rballoc(SOF_MEM_FLAG_USER, size);
}

void webrtc_ns2_free(void *ptr)
{
	if (!ptr)
		return;
	if (active_ns2_mod) {
		mod_free(active_ns2_mod, ptr);
		return;
	}
	rfree(ptr);
}

void *webrtc_ns2_calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;

	if (total == 0)
		return NULL;
	if (active_ns2_mod)
		return mod_zalloc(active_ns2_mod, total);
	void *p = rballoc(SOF_MEM_FLAG_USER, total);

	if (p)
		memset(p, 0, total);
	return p;
}

#if defined(LLEXT_MODULE)
/* ============================ Memory ============================ */

void *malloc(size_t size)
{
	return webrtc_ns2_malloc(size);
}

void free(void *ptr)
{
	webrtc_ns2_free(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
	return webrtc_ns2_calloc(nmemb, size);
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

/* ============================ Math ============================== */

static inline float ns2_ldexp2(int n)
{
	union { float f; uint32_t i; } u;

	if (n < -126)
		return 0.0f;
	if (n > 127)
		n = 127;
	u.i = (uint32_t)(n + 127) << 23;
	return u.f;
}

static float ns2_log2f(float x)
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

float log10f(float x)
{
	return ns2_log2f(x) * 0.3010299956639812f;
}

double log10(double x)
{
	return (double)log10f((float)x);
}

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

static inline float ns2_roundf(float x)
{
	return (x >= 0.0f) ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}

float fabsf(float x)
{
	return (x < 0.0f) ? -x : x;
}

double fabs(double x)
{
	return (x < 0.0) ? -x : x;
}

float sqrtf(float x)
{
	if (x <= 0.0f)
		return 0.0f;
	return __builtin_sqrtf(x);
}

double sqrt(double x)
{
	return (double)sqrtf((float)x);
}

static inline void ns2_sincosf_poly(float z, float *s_out, float *c_out)
{
	float z2 = z * z;

	*s_out = z * (1.0f + z2 * (-0.166666667f + z2 * (0.008333333f - z2 * 0.0001984127f)));
	*c_out = 1.0f + z2 * (-0.5f + z2 * (0.041666667f + z2 * (-0.0013888889f + z2 * 0.0000248016f)));
}

float sinf(float x)
{
	const float inv_half_pi = 0.636619772367581343f;
	const float half_pi_hi  = 1.5707963267948966f;
	int k = (int)ns2_roundf(x * inv_half_pi);
	float z = x - (float)k * half_pi_hi;
	float s, c;

	ns2_sincosf_poly(z, &s, &c);

	switch ((k % 4 + 4) % 4) {
	case 0: return s;
	case 1: return c;
	case 2: return -s;
	case 3: default: return -c;
	}
}

double sin(double x)
{
	return (double)sinf((float)x);
}

float cosf(float x)
{
	const float inv_half_pi = 0.636619772367581343f;
	const float half_pi_hi  = 1.5707963267948966f;
	int k = (int)ns2_roundf(x * inv_half_pi);
	float z = x - (float)k * half_pi_hi;
	float s, c;

	ns2_sincosf_poly(z, &s, &c);

	switch ((k % 4 + 4) % 4) {
	case 0: return c;
	case 1: return -s;
	case 2: return -c;
	case 3: default: return s;
	}
}

double cos(double x)
{
	return (double)cosf((float)x);
}

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
	return p * ns2_ldexp2(n);
}

float expf(float x)
{
	return exp2f(x * 1.4426950408889634f);
}

double exp(double x)
{
	return (double)expf((float)x);
}
#endif /* LLEXT_MODULE */

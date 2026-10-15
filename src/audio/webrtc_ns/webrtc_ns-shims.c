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
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/alloc.h>

static struct processing_module *active_ns_mod;

void webrtc_ns_set_active_mod(struct processing_module *mod)
{
	active_ns_mod = mod;
}

#if defined(LLEXT_MODULE)
/* ============================ Memory ============================ */

void *malloc(size_t size)
{
	if (active_ns_mod)
		return mod_alloc(active_ns_mod, size);
	return rballoc(SOF_MEM_FLAG_USER, size);
}

void free(void *ptr)
{
	if (active_ns_mod) {
		mod_free(active_ns_mod, ptr);
		return;
	}
	rfree(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;

	if (active_ns_mod)
		return mod_zalloc(active_ns_mod, total);
	void *p = rballoc(SOF_MEM_FLAG_USER, total);

	if (p)
		memset(p, 0, total);
	return p;
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


float fabsf(float x)
{
	return (x < 0.0f) ? -x : x;
}

double fabs(double x)
{
	return (x < 0.0) ? -x : x;
}

static inline float ns_exp2f(float x)
{
	if (x > 127.0f) return 3.40282347e+38f;
	if (x < -126.0f) return 0.0f;
	int n = (int)x;
	if (x < 0.0f && (float)n != x) n--;
	float f = x - (float)n;
	float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * 0.0555041f));
	union { float f; uint32_t i; } u;
	if (n < -126) return 0.0f;
	if (n > 127) n = 127;
	u.i = (uint32_t)(n + 127) << 23;
	return p * u.f;
}

float expf(float x)
{
	return ns_exp2f(x * 1.4426950408889634f);
}

double exp(double x)
{
	return (double)expf((float)x);
}

double tanh(double x)
{
	if (x > 10.0) return 1.0;
	if (x < -10.0) return -1.0;
	double e2 = exp(2.0 * x);
	return (e2 - 1.0) / (e2 + 1.0);
}

float atanf(float x)
{
	if (x == 1.0f)
		return 0.7853981633974483f;
	float x2 = x * x;
	return x * (1.0f + 0.28f * x2) / (1.0f + 0.613f * x2);
}

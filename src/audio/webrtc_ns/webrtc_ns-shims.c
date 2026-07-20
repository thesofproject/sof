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
#include <rtos/alloc.h>

/* ============================ Memory ============================ */

void *malloc(size_t size)
{
	return rballoc(SOF_MEM_FLAG_USER, size);
}

void free(void *ptr)
{
	rfree(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
	void *p = rballoc(SOF_MEM_FLAG_USER, nmemb * size);

	if (p)
		memset(p, 0, nmemb * size);
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

/* ============================ Math ============================== */
/*
 * The NS spectral processing uses log(), exp(), sqrt(), pow(), fabsf().
 * Route to sofm_ equivalents where available; fall back to soft-float
 * versions provided by the Zephyr minimal libc for others.
 */
#include <sof/math/numbers.h>

float sqrtf(float x) { return (float)sofm_sqrt_int32((int32_t)(x * 65536.0f)) / 256.0f; }

/* log/exp/pow are used for spectral gain computation — leave as weak
 * references satisfied by the toolchain's soft-float libm linked into
 * the SOF binary. LLEXT resolves them at load time from the core symbol
 * table. These shims are intentionally empty for now; add implementations
 * if the real WebRTC NS backend needs them unsatisfied. */

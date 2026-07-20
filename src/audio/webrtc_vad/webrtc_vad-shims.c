// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Local libc surface for the webrtc_vad libfvad backend in LLEXT context.
//
// libfvad references a small set of libc symbols that the SOF core does not
// export to LLEXT modules. This file defines them inside the module so it
// resolves at load time. Unlike the ffmpeg_dec shims, libfvad's requirements
// are minimal — it only needs memory allocation and a handful of string/math
// helpers.
//
// NOTE: this file intentionally includes NO libc headers to avoid clashing
// with newlib prototypes; only the symbol names matter to the linker.

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

int memcmp(const void *s1, const void *s2, size_t n)
{
	const uint8_t *a = s1, *b = s2;

	while (n--) {
		if (*a != *b)
			return *a - *b;
		a++; b++;
	}
	return 0;
}

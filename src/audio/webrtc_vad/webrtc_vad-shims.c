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
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/alloc.h>
#if !defined(LLEXT_MODULE)
#include <rtos/userspace_helper.h>
static APP_SYSUSER_BSS struct processing_module *active_vad_mod;
#else
static struct processing_module *active_vad_mod;
#endif

void webrtc_vad_set_active_mod(struct processing_module *mod)
{
	active_vad_mod = mod;
}

#if defined(LLEXT_MODULE)
/* ============================ Memory ============================ */

void *malloc(size_t size)
{
	if (active_vad_mod)
		return mod_alloc(active_vad_mod, size);
	return rballoc(SOF_MEM_FLAG_USER, size);
}

void free(void *ptr)
{
	if (active_vad_mod) {
		mod_free(active_vad_mod, ptr);
		return;
	}
	rfree(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;

	if (active_vad_mod)
		return mod_zalloc(active_vad_mod, total);
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
#endif /* LLEXT_MODULE */

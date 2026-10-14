// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// malloc/free/realloc for the ffmpeg_dec libavcodec backend, backed by the SOF
// module heap. libavcodec's av_malloc() bottoms out in libc malloc; the SOF core
// does not export malloc to LLEXT modules, so we provide it here and route it to
// mod_alloc_align()/mod_free() — no large static reservation, memory comes from
// the DSP heap like any other SOF module.
//
// Each block is prefixed with a small header storing its payload size so realloc
// can copy the old contents (SOF's allocator does not expose a realloc). The
// header is 16 bytes to preserve 16-byte alignment of the returned pointer.
//
// The active processing_module is bound at the entry of every backend op (init/
// prepare/process/reset/free) via ffmpeg_dec_libc_bind(); decode is serialized
// per module on a pipeline core, so a single global binding is safe.

#include <sof/audio/module_adapter/module/generic.h>
#include <zephyr/arch/arch_inlines.h>
#include <rtos/string.h>
#include <stddef.h>
#include <stdint.h>
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define FFMPEG_DEC_ALLOC_HDR	16

struct ffmpeg_dec_block_hdr {
	struct processing_module *mod;
	size_t size;
};

#ifndef CONFIG_CORE_COUNT
#define CONFIG_CORE_COUNT 4
#endif

static struct processing_module *ffmpeg_dec_alloc_mod[CONFIG_CORE_COUNT];

static inline int ffmpeg_dec_get_core(void)
{
#if CONFIG_MULTICORE && CONFIG_SMP
	return (int)arch_proc_id();
#else
	return 0;
#endif
}

void ffmpeg_dec_libc_bind(struct processing_module *mod)
{
	int core = ffmpeg_dec_get_core();

	if (core >= 0 && core < CONFIG_CORE_COUNT)
		ffmpeg_dec_alloc_mod[core] = mod;
}

size_t ffmpeg_dec_heap_free(void)
{
	return 0;
}

size_t ffmpeg_dec_heap_lastfail(void)
{
	return 0;
}

void *malloc(size_t size)
{
	int core = ffmpeg_dec_get_core();
	struct processing_module *mod;
	uint8_t *base;
	struct ffmpeg_dec_block_hdr *hdr;

	if (core < 0 || core >= CONFIG_CORE_COUNT)
		return NULL;

	mod = ffmpeg_dec_alloc_mod[core];
	if (!mod || !size)
		return NULL;

	base = mod_alloc_align(mod, size + FFMPEG_DEC_ALLOC_HDR,
			       FFMPEG_DEC_ALLOC_HDR);
	if (!base)
		return NULL;

	hdr = (struct ffmpeg_dec_block_hdr *)base;
	hdr->mod = mod;
	hdr->size = size;

	return base + FFMPEG_DEC_ALLOC_HDR;
}

void free(void *ptr)
{
	struct ffmpeg_dec_block_hdr *hdr;

	if (!ptr)
		return;

	hdr = (struct ffmpeg_dec_block_hdr *)((uint8_t *)ptr - FFMPEG_DEC_ALLOC_HDR);
	if (hdr->mod)
		mod_free(hdr->mod, hdr);
}

void *realloc(void *ptr, size_t size)
{
	struct ffmpeg_dec_block_hdr *hdr;
	size_t old_size;
	void *new_ptr;

	if (!ptr)
		return malloc(size);

	if (!size) {
		free(ptr);
		return NULL;
	}

	hdr = (struct ffmpeg_dec_block_hdr *)((uint8_t *)ptr - FFMPEG_DEC_ALLOC_HDR);
	old_size = hdr->size;
	new_ptr = malloc(size);
	if (new_ptr) {
		memcpy(new_ptr, ptr, old_size < size ? old_size : size);
		free(ptr);
	}

	return new_ptr;
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	void *ptr = malloc(total);

	if (ptr)
		memset(ptr, 0, total);
	return ptr;
}

/* av_malloc may use posix_memalign. Our allocator returns 16-byte-aligned blocks,
 * which is sufficient for the --disable-asm (no-SIMD) build; larger alignment
 * requests are satisfied at 16 bytes. */
int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *ptr = malloc(size);

	(void)alignment;
	if (!ptr)
		return 12;	/* ENOMEM */
	*memptr = ptr;
	return 0;
}

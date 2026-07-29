// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Built-in (=y) heap for the ffmpeg_dec libavcodec backend.
//
// FFmpeg av_malloc() bottoms out in plain malloc()/free()/realloc()/calloc()
// (this build has HAVE_POSIX_MEMALIGN/MEMALIGN/ALIGNED_MALLOC all 0, and
// av_malloc does no self-alignment, so malloc() must return ALIGN-aligned
// memory -- ALIGN is at most 64 here). By default the =y image resolves those
// to Zephyr common-libc malloc, backed by the CONFIG_COMMON_LIBC_MALLOC_ARENA
// arena, which lives in app_smem and counts against rimage image_size, so it
// cannot practically exceed ~512 KiB. AAC-LC needs a single ~541 KiB allocation
// (libavutil av_tx float MDCT tables) that does not fit, giving -ENOMEM.
//
// We must NOT route these through the SOF per-module allocator (mod_alloc):
// with CONFIG_SOF_FULL_ZEPHYR_APPLICATION + CONFIG_USERSPACE, mod_alloc_ext()
// is a Zephyr __syscall that takes a K_FOREVER mutex and allocates a tracking
// container per call, and it draws from the small per-module heap -- driving
// every libav malloc through it on the DP thread stalls avcodec_open2().
//
// Instead we carve a dedicated FFmpeg heap out of a static buffer. It lands in
// .bss (NOBITS), so it does NOT count against the signed-image window and is
// NOT part of the 1 MiB SOF allocator mem_zone -- it lives in the general ram
// region (several MiB) and is sized to hold the AAC-LC working set. A Zephyr
// sys_heap runs over it: no syscall, no mutex, no per-alloc tracking. The
// linker --wrap (see CMakeLists.txt) binds every malloc/free/realloc/calloc in
// the image to the __wrap_* below; whether a pointer belongs to us is a simple
// address-range test, so pre-bind boot allocations (common-libc arena) and our
// allocations coexist and free correctly regardless of order.

#include <sof/audio/module_adapter/module/generic.h>
#include <zephyr/sys/sys_heap.h>
#include <zephyr/sys/mem_stats.h>
#include <rtos/string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ffmpeg_dec.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL); /*DBG*/

/* av_malloc's ALIGN is at most 64 in this build; satisfy it for every block. */
#define FFMPEG_DEC_ALLOC_ALIGN	64
/* Working-set heap: one ~541 KiB av_tx MDCT table alloc + decoder context /
 * channel elements. 768 KiB gives comfortable headroom. */
#define FFMPEG_DEC_HEAP_BYTES	(880 * 1024)

extern void *__real_malloc(size_t size);
extern void __real_free(void *ptr);
extern void *__real_realloc(void *ptr, size_t size);

static uint8_t ffmpeg_dec_heap_buf[FFMPEG_DEC_HEAP_BYTES]
	__aligned(FFMPEG_DEC_ALLOC_ALIGN);
static struct sys_heap ffmpeg_dec_heap;
static bool ffmpeg_dec_heap_ready;
static size_t ffmpeg_dec_last_fail; /*DBG*/

static inline void ffmpeg_dec_heap_ensure(void)
{
	if (!ffmpeg_dec_heap_ready) {
		sys_heap_init(&ffmpeg_dec_heap, ffmpeg_dec_heap_buf,
			      sizeof(ffmpeg_dec_heap_buf));
		ffmpeg_dec_heap_ready = true;
	}
}

void ffmpeg_dec_libc_bind(struct processing_module *mod)
{
	(void)mod;
	ffmpeg_dec_heap_ensure();
}

/* Free bytes currently available in the FFmpeg sys_heap (0 before init). */
size_t ffmpeg_dec_heap_free(void)
{
#if CONFIG_SYS_HEAP_RUNTIME_STATS
	struct sys_memory_stats st;

	if (!ffmpeg_dec_heap_ready)
		return 0;
	if (sys_heap_runtime_stats_get(&ffmpeg_dec_heap, &st) < 0)
		return 0;
	return st.free_bytes;
#else
	return 0;
#endif
}

size_t ffmpeg_dec_heap_lastfail(void) /*DBG*/
{
	return ffmpeg_dec_last_fail;
}

static inline bool ffmpeg_dec_is_ours(const void *ptr)
{
	const uint8_t *p = ptr;

	return p >= ffmpeg_dec_heap_buf &&
	       p < ffmpeg_dec_heap_buf + sizeof(ffmpeg_dec_heap_buf);
}

void *__wrap_malloc(size_t size)
{
	if (!size)
		return NULL;

	/*
	 * Lazy-init on first use so that EVERY malloc in the image (including
	 * pre-bind boot allocations) is served from our heap. This makes the
	 * common-libc arena dead weight, so it can be shrunk to a token size
	 * and the reclaimed ucram handed to this heap instead.
	 */
	ffmpeg_dec_heap_ensure();

	{
		void *p = sys_heap_aligned_alloc(&ffmpeg_dec_heap,
						 FFMPEG_DEC_ALLOC_ALIGN, size);
		if (!p)
			ffmpeg_dec_last_fail = size;
		return p;
	}
}

void __wrap_free(void *ptr)
{
	if (!ptr)
		return;

	if (ffmpeg_dec_is_ours(ptr))
		sys_heap_free(&ffmpeg_dec_heap, ptr);
	else
		__real_free(ptr);
}

void *__wrap_realloc(void *ptr, size_t size)
{
	if (!ptr)
		return __wrap_malloc(size);

	/* Not one of ours (e.g. allocated pre-bind): defer to the real realloc. */
	if (!ffmpeg_dec_is_ours(ptr))
		return __real_realloc(ptr, size);

	if (!size) {
		sys_heap_free(&ffmpeg_dec_heap, ptr);
		return NULL;
	}

	return sys_heap_aligned_realloc(&ffmpeg_dec_heap, ptr,
					FFMPEG_DEC_ALLOC_ALIGN, size);
}

void *__wrap_calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	void *ptr = __wrap_malloc(total);

	if (ptr)
		memset(ptr, 0, total);
	return ptr;
}

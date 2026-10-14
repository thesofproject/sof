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
// Under CONFIG_SOF_USERSPACE_APPLICATION=y (e.g. PTL built with os_linux_overlay.conf /
// ll_userspace_overlay.conf, where DP audio pipelines run in user-space threads),
// module_adapter_mem_alloc() creates a dedicated per-instance user-space vregion
// sized from the topology's heap_bytes. In that configuration (whether built as =m
// via ffmpeg_dec-alloc.c or =y here), we route allocations through the user-space
// safe mod_alloc_align()/mod_free() syscalls backed by the module's vregion.
//
// Under kernel-mode =y builds (!CONFIG_SOF_USERSPACE_APPLICATION, e.g. non-userspace
// targets or CONFIG_SOF_USERSPACE_PROXY=y where built-in modules have mod->user_ctx == NULL
// and execute in supervisor threads, and CONFIG_SOF_USERSPACE_USE_DRIVER_HEAP=y bypasses
// per-module vregion creation so mod_alloc draws from the 1 MiB shared sof_heap), we
// carve a dedicated FFmpeg heap out of a static .bss buffer (NOBITS) and run a Zephyr
// sys_heap over it.

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/userspace_helper.h>
#include <zephyr/sys/sys_heap.h>
#include <zephyr/sys/mem_stats.h>
#include <zephyr/spinlock.h>
#include <rtos/string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ffmpeg_dec.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL); /*DBG*/

/* av_malloc's ALIGN is at most 64 in this build; satisfy it for every block. */
#define FFMPEG_DEC_ALLOC_ALIGN	64

extern void *__real_malloc(size_t size);
extern void __real_free(void *ptr);
extern void *__real_realloc(void *ptr, size_t size);

#if defined(CONFIG_SOF_USERSPACE_APPLICATION)

#define FFMPEG_DEC_BLOCK_MAGIC	0x46464d50U /* 'FFMP' */

struct ffmpeg_dec_block_hdr {
	uint32_t magic;
	struct processing_module *mod;
	size_t size;
};

static APP_TASK_BSS struct processing_module *ffmpeg_dec_alloc_mod;
static APP_TASK_BSS size_t ffmpeg_dec_last_fail;

void ffmpeg_dec_libc_bind(struct processing_module *mod)
{
	ffmpeg_dec_alloc_mod = mod;
}

size_t ffmpeg_dec_heap_free(void)
{
	return 0;
}

size_t ffmpeg_dec_heap_lastfail(void)
{
	return ffmpeg_dec_last_fail;
}

void *__wrap_malloc(size_t size)
{
	struct processing_module *mod = ffmpeg_dec_alloc_mod;
	struct ffmpeg_dec_block_hdr *hdr;
	uint8_t *base;

	if (!size)
		return NULL;

	if (!mod)
		return __real_malloc(size);

	base = mod_alloc_align(mod, size + FFMPEG_DEC_ALLOC_ALIGN,
			       FFMPEG_DEC_ALLOC_ALIGN);
	if (!base) {
		ffmpeg_dec_last_fail = size;
		return NULL;
	}

	hdr = (struct ffmpeg_dec_block_hdr *)base;
	hdr->magic = FFMPEG_DEC_BLOCK_MAGIC;
	hdr->mod = mod;
	hdr->size = size;

	return base + FFMPEG_DEC_ALLOC_ALIGN;
}

void __wrap_free(void *ptr)
{
	struct ffmpeg_dec_block_hdr *hdr;

	if (!ptr)
		return;

	if (!ffmpeg_dec_alloc_mod) {
		__real_free(ptr);
		return;
	}

	hdr = (struct ffmpeg_dec_block_hdr *)((uint8_t *)ptr - FFMPEG_DEC_ALLOC_ALIGN);
	if (hdr->magic == FFMPEG_DEC_BLOCK_MAGIC && hdr->mod)
		mod_free(hdr->mod, hdr);
	else
		__real_free(ptr);
}

void *__wrap_realloc(void *ptr, size_t size)
{
	struct ffmpeg_dec_block_hdr *hdr;
	size_t old_size;
	void *new_ptr;

	if (!ptr)
		return __wrap_malloc(size);

	if (!ffmpeg_dec_alloc_mod)
		return __real_realloc(ptr, size);

	if (!size) {
		__wrap_free(ptr);
		return NULL;
	}

	hdr = (struct ffmpeg_dec_block_hdr *)((uint8_t *)ptr - FFMPEG_DEC_ALLOC_ALIGN);
	if (hdr->magic != FFMPEG_DEC_BLOCK_MAGIC)
		return __real_realloc(ptr, size);

	old_size = hdr->size;
	new_ptr = __wrap_malloc(size);
	if (new_ptr) {
		memcpy(new_ptr, ptr, old_size < size ? old_size : size);
		__wrap_free(ptr);
	}

	return new_ptr;
}

#else /* !CONFIG_SOF_USERSPACE_APPLICATION */

/* Working-set heap: AAC-LC needs ~768 KiB (one ~541 KiB av_tx MDCT table alloc +
 * decoder context). FLAC/MP3/Opus only need a small fraction of this, so sizing
 * to 576 KiB reclaims over 300 KiB for sof_heap to accommodate the 64 KiB DP stack.
 */
#if defined(CONFIG_FFMPEG_DEC_AAC)
#define FFMPEG_DEC_HEAP_BYTES	(880 * 1024)
#else
#define FFMPEG_DEC_HEAP_BYTES	(576 * 1024)
#endif

static uint8_t ffmpeg_dec_heap_buf[FFMPEG_DEC_HEAP_BYTES]
	__aligned(FFMPEG_DEC_ALLOC_ALIGN);
static struct sys_heap ffmpeg_dec_heap;
static bool ffmpeg_dec_heap_ready;
static struct k_spinlock ffmpeg_dec_heap_lock;
static size_t ffmpeg_dec_last_fail; /*DBG*/

static inline void ffmpeg_dec_heap_ensure_locked(void)
{
	if (!ffmpeg_dec_heap_ready) {
		sys_heap_init(&ffmpeg_dec_heap, ffmpeg_dec_heap_buf,
			      sizeof(ffmpeg_dec_heap_buf));
		ffmpeg_dec_heap_ready = true;
	}
}

void ffmpeg_dec_libc_bind(struct processing_module *mod)
{
	k_spinlock_key_t key;

	(void)mod;
	key = k_spin_lock(&ffmpeg_dec_heap_lock);
	ffmpeg_dec_heap_ensure_locked();
	k_spin_unlock(&ffmpeg_dec_heap_lock, key);
}

/* Free bytes currently available in the FFmpeg sys_heap (0 before init). */
size_t ffmpeg_dec_heap_free(void)
{
#if CONFIG_SYS_HEAP_RUNTIME_STATS
	struct sys_memory_stats st;
	k_spinlock_key_t key;
	int ret;

	key = k_spin_lock(&ffmpeg_dec_heap_lock);
	if (!ffmpeg_dec_heap_ready) {
		k_spin_unlock(&ffmpeg_dec_heap_lock, key);
		return 0;
	}
	ret = sys_heap_runtime_stats_get(&ffmpeg_dec_heap, &st);
	k_spin_unlock(&ffmpeg_dec_heap_lock, key);
	if (ret < 0)
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
	k_spinlock_key_t key;
	void *p;

	if (!size)
		return NULL;

	/*
	 * Lazy-init on first use so that EVERY malloc in the image (including
	 * pre-bind boot allocations) is served from our heap. This makes the
	 * common-libc arena dead weight, so it can be shrunk to a token size
	 * and the reclaimed ucram handed to this heap instead.
	 */
	key = k_spin_lock(&ffmpeg_dec_heap_lock);
	ffmpeg_dec_heap_ensure_locked();
	p = sys_heap_aligned_alloc(&ffmpeg_dec_heap,
				   FFMPEG_DEC_ALLOC_ALIGN, size);
	if (!p)
		ffmpeg_dec_last_fail = size;
	k_spin_unlock(&ffmpeg_dec_heap_lock, key);

	return p;
}

void __wrap_free(void *ptr)
{
	k_spinlock_key_t key;

	if (!ptr)
		return;

	if (ffmpeg_dec_is_ours(ptr)) {
		key = k_spin_lock(&ffmpeg_dec_heap_lock);
		sys_heap_free(&ffmpeg_dec_heap, ptr);
		k_spin_unlock(&ffmpeg_dec_heap_lock, key);
	} else {
		__real_free(ptr);
	}
}

void *__wrap_realloc(void *ptr, size_t size)
{
	k_spinlock_key_t key;
	void *p;

	if (!ptr)
		return __wrap_malloc(size);

	/* Not one of ours (e.g. allocated pre-bind): defer to the real realloc. */
	if (!ffmpeg_dec_is_ours(ptr))
		return __real_realloc(ptr, size);

	if (!size) {
		__wrap_free(ptr);
		return NULL;
	}

	key = k_spin_lock(&ffmpeg_dec_heap_lock);
	p = sys_heap_aligned_realloc(&ffmpeg_dec_heap, ptr,
				     FFMPEG_DEC_ALLOC_ALIGN, size);
	k_spin_unlock(&ffmpeg_dec_heap_lock, key);
	return p;
}

#endif /* CONFIG_SOF_USERSPACE_APPLICATION */

void *__wrap_calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	void *ptr = __wrap_malloc(total);

	if (ptr)
		memset(ptr, 0, total);
	return ptr;
}

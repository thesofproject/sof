// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Memory allocator shims for the WebRTC AECm module.
//
// WebRTC AECm and its dependencies (ring_buffer, delay_estimator) use
// malloc, calloc, realloc, and free. Since SOF uses rballoc/rfree from its
// native heap rather than Zephyr's libc malloc arena, these shims map all
// WebRTC allocations to SOF_MEM_FLAG_USER heap memory.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <rtos/alloc.h>

#define WEBRTC_MEM_MAGIC 0xAEC11111

struct __attribute__((aligned(8))) webrtc_mem_hdr {
	size_t   size;
	uint32_t magic;
	uint32_t pad;
};

void *webrtc_malloc(size_t size)
{
	struct webrtc_mem_hdr *hdr;

	if (size == 0)
		return NULL;

	hdr = sof_heap_alloc(sof_sys_heap_get(), SOF_MEM_FLAG_USER, size + sizeof(*hdr), 8);
	if (!hdr)
		return NULL;

	hdr->size = size;
	hdr->magic = WEBRTC_MEM_MAGIC;
	hdr->pad = 0;

	return (void *)(hdr + 1);
}

void webrtc_free(void *ptr)
{
	struct webrtc_mem_hdr *hdr;

	if (!ptr)
		return;

	hdr = ((struct webrtc_mem_hdr *)ptr) - 1;
	if (hdr->magic == WEBRTC_MEM_MAGIC) {
		hdr->magic = 0;
		sof_heap_free(sof_sys_heap_get(), hdr);
	} else {
		sof_heap_free(sof_sys_heap_get(), ptr);
	}
}

void *webrtc_calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	void *p = webrtc_malloc(total);

	if (p)
		memset(p, 0, total);
	return p;
}

void *webrtc_realloc(void *ptr, size_t size)
{
	struct webrtc_mem_hdr *hdr;
	void *new_ptr;
	size_t copy_size;

	if (!ptr)
		return webrtc_malloc(size);

	if (size == 0) {
		webrtc_free(ptr);
		return NULL;
	}

	hdr = ((struct webrtc_mem_hdr *)ptr) - 1;
	new_ptr = webrtc_malloc(size);
	if (!new_ptr)
		return NULL;

	if (hdr->magic == WEBRTC_MEM_MAGIC) {
		copy_size = (size < hdr->size) ? size : hdr->size;
		memcpy(new_ptr, ptr, copy_size);
		webrtc_free(ptr);
	} else {
		memcpy(new_ptr, ptr, size);
		webrtc_free(ptr);
	}

	return new_ptr;
}

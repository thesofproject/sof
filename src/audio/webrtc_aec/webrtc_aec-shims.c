// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Memory allocator shims for WebRTC modules.
//
// Maps malloc/calloc/free to SOF's rmalloc/rzalloc/rfree on SOF_MEM_FLAG_USER heap.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <rtos/alloc.h>
#include <rtos/panic.h>

void *webrtc_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	return rmalloc(SOF_MEM_FLAG_USER, size);
}

void webrtc_free(void *ptr)
{
	if (!ptr)
		return;

	rfree(ptr);
}

void *webrtc_calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;

	if (total == 0)
		return NULL;

	return rzalloc(SOF_MEM_FLAG_USER, total);
}

void *webrtc_realloc(void *ptr, size_t size)
{
	if (!ptr)
		return webrtc_malloc(size);

	if (size == 0) {
		webrtc_free(ptr);
		return NULL;
	}

	/* Not called by AEC3 */
	k_panic();
	return NULL;
}

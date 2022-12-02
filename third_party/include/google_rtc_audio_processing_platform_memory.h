// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Google LLC.
//
// Author: Per Ahgren <peah@google.com>
#ifndef GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_MEMORY_H_
#define GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_MEMORY_H_

#include <stddef.h>

// Provides functions facilitating memory allocation and deallocation.
// This header only provides the definition of the methods, and it is up to the
// platform to provide the implementations.

#ifdef __cplusplus
extern "C" {
#endif

// Allocates memory of size `size` and returns a pointer to the allocated
// memory. If no memory is available, NULL is returned.
void* GoogleRtcMalloc(size_t size);

// Frees the memory pointed to by `p`.
void GoogleRtcFree(void* p);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_MEMORY_H_

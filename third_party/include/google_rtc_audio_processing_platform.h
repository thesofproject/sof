// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC.
//
// Author: Lionel Koenig <lionelk@google.com>
#ifndef GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_H_
#define GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Allocates memory of size `size` and returns a pointer to the allocated
// memory. If no memory is available, NULL is returned.
void *GoogleRtcMalloc(size_t size);

// Frees the memory pointed to by `p`.
void GoogleRtcFree(void *p);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_H_

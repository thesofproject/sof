/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Google LLC.
 *
 * Author: Kehuang Li <kehuangli@google.com>
 */

#ifndef GOOGLE_AUDIO_POST_PROCESSING_PLATFORM_H_
#define GOOGLE_AUDIO_POST_PROCESSING_PLATFORM_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Allocates memory of size `size` and returns a pointer to the allocated
// memory. If no memory is available, NULL is returned.
void *GoogleAudioPostProcessingMalloc(size_t size);

// Frees the memory pointed to by `p`.
void GoogleAudioPostProcessingFree(void *p);

// Log "line: code".
void GoogleAudioPostProcessingLog(int line, int code);

#ifdef __cplusplus
}
#endif

#endif // GOOGLE_AUDIO_POST_PROCESSING_PLATFORM_H_

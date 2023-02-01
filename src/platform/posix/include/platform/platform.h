/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef PLATFORM_POSIX_PLATFORM_PLATFORM_H
#define PLATFORM_POSIX_PLATFORM_PLATFORM_H

#include <platform/lib/memory.h>

#define DMA_TRACE_LOCAL_SIZE 8192

#define HOST_PAGE_SIZE 4096

#define PLATFORM_MAX_CHANNELS 8

#define PLATFORM_MAX_STREAMS 16

#define DMA_TRACE_PERIOD 500000
#define DMA_TRACE_RESCHEDULE_TIME 500000

#define DMA_FLUSH_TRACE_SIZE (MAILBOX_TRACE_SIZE >> 2)

#define PLATFORM_DEFAULT_CLOCK 0

#define PLATFORM_DEFAULT_DELAY 12

struct sof;

void posix_dma_init(struct sof *sof);
void posix_dai_init(struct sof *sof);

#endif /* PLATFORM_POSIX_PLATFORM_PLATFORM_H */

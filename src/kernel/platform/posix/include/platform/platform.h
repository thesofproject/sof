/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef PLATFORM_POSIX_PLATFORM_PLATFORM_H
#define PLATFORM_POSIX_PLATFORM_PLATFORM_H

#include <platform/lib/memory.h>

#define HOST_PAGE_SIZE 4096

#define PLATFORM_DEFAULT_CLOCK 0

#define HW_CFG_VERSION 0x010000

struct sof;

void posix_dma_init(struct sof *sof);

#endif /* PLATFORM_POSIX_PLATFORM_PLATFORM_H */

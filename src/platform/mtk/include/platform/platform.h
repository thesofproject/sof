/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef _SOF_PLATFORM_MTK_PLATFORM_H
#define _SOF_PLATFORM_MTK_PLATFORM_H

#include <platform/lib/clk.h>

#define PLATFORM_MAX_CHANNELS 4
#define PLATFORM_MAX_STREAMS 5

#define HOST_PAGE_SIZE 4096

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

#endif /* _SOF_PLATFORM_MTK_PLATFORM_H */

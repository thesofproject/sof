/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef PLATFORM_HOST_PLATFORM_MEMORY_H
#define PLATFORM_HOST_PLATFORM_MEMORY_H

#include <stdint.h>
#include <ipc/header.h>

#include <platform/lib/mailbox_layout.h>

#define PLATFORM_HEAP_SYSTEM 1
#define PLATFORM_HEAP_SYSTEM_RUNTIME 1
#define PLATFORM_HEAP_RUNTIME 1
#define PLATFORM_HEAP_BUFFER 1

#define host_to_local(addr) (addr)

#endif /* PLATFORM_HOST_PLATFORM_MEMORY_H */

/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

/**
 * Symbols shared between the native_posix fuzz harness
 * (src/platform/posix/fuzz.c) and the SOF posix IPC layer
 * (src/platform/posix/ipc.c).
 *
 * Defining them in one place avoids type-mismatch bugs (e.g. a single
 * `extern uint8_t *posix_fuzz_buf, posix_fuzz_sz;` declaring `posix_fuzz_sz`
 * as a `uint8_t` rather than a `size_t`).
 */

#ifndef PLATFORM_POSIX_FUZZ_H
#define PLATFORM_POSIX_FUZZ_H

#include <stddef.h>
#include <stdint.h>

extern const uint8_t *posix_fuzz_buf;
extern size_t posix_fuzz_sz;

#endif /* PLATFORM_POSIX_FUZZ_H */

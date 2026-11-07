/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2026 Intel Corporation. All rights reserved. */

#ifndef PLATFORM_POSIX_FUZZ_H
#define PLATFORM_POSIX_FUZZ_H

#include <stdbool.h>

/**
 * @brief Reset IPC staging state at the start of a new fuzz testcase.
 *
 * Must be called before staging new fuzz input so that any leftover
 * state from a previous testcase that failed to drain within the tick
 * budget cannot influence the new one.
 */
void posix_fuzz_case_begin(void);

/**
 * @brief Query whether any staged IPC fuzz input is still pending.
 *
 * @return true if the raw input buffer or the IPC staging queue is
 *         non-empty, false when fully drained.
 */
bool posix_fuzz_case_pending(void);

/**
 * @brief Hard-reset all staged IPC fuzz state.
 *
 * Called when the simulator tick budget is exhausted before the input
 * has been fully drained, ensuring the next testcase starts clean.
 */
void posix_fuzz_case_abort(void);

#endif /* PLATFORM_POSIX_FUZZ_H */

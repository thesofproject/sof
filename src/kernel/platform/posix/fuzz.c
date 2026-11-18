// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2024 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <irq_ctrl.h>
#include <zephyr/sys/time_units.h>
#include <nsi_cpu_if.h>
#include <nsi_main_semipublic.h>
#include <platform/fuzz.h>

const uint8_t *posix_fuzz_buf;
size_t posix_fuzz_sz;

/* Number of simulator quanta the budget is split into for the
 * drain-or-abort loop. More quanta = earlier exit on quick testcases.
 */
#define POSIX_FUZZ_DRAIN_QUANTA 8

/**
 * Entry point for fuzzing. Works by placing the data
 * into two known symbols, triggering an app-visible interrupt, and
 * then letting the simulator run for up to a fixed amount of time
 * split into small quanta, exiting as soon as the OS has drained the
 * staged fuzz input. If the budget is exhausted before drain we drop
 * pending state to keep testcases isolated.
 */
NATIVE_SIMULATOR_IF
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t sz)
{
	static bool runner_initialized;
	uint64_t total_us;
	uint64_t quantum_us;
	int i;

	if (!runner_initialized) {
		nsi_init(0, NULL);
		runner_initialized = true;
	}

	total_us = k_ticks_to_us_ceil64(CONFIG_ZEPHYR_POSIX_FUZZ_TICKS);
	quantum_us = (total_us + POSIX_FUZZ_DRAIN_QUANTA - 1) / POSIX_FUZZ_DRAIN_QUANTA;
	if (quantum_us == 0)
		quantum_us = 1;

	/* Fresh testcase: drop any leftovers from a previous case before
	 * staging the new buffer, so state from the prior input cannot
	 * influence this one.
	 */
	posix_fuzz_case_begin();

	/* Provide the fuzz data to the embedded OS as an interrupt, with
	 * "DMA-like" data placed into posix_fuzz_buf/sz.
	 */
	posix_fuzz_buf = data;
	posix_fuzz_sz = sz;
	hw_irq_ctrl_set_irq(CONFIG_ZEPHYR_POSIX_FUZZ_IRQ);

	/* Bounded drain loop: run simulator in small quanta and exit
	 * as soon as both the raw input buffer and the staged IPC
	 * queue are empty.
	 */
	for (i = 0; i < POSIX_FUZZ_DRAIN_QUANTA; i++) {
		nsi_exec_for(quantum_us);
		if (!posix_fuzz_case_pending())
			return 0;
	}

	/* Budget exhausted without full drain: hard reset so the next
	 * testcase starts clean.
	 */
	posix_fuzz_case_abort();
	return 0;
}

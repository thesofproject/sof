/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/**
 * Should be included from sof/schedule/task.h
 * but triggers include chain issue
 * FIXME
 */
int sof_main(int argc, char *argv[]);

/**
 * TODO: Here comes SOF initialization
 */

void main(void)
{
	int ret;

	LOG_INF("SOF on %s", CONFIG_BOARD);

	/* sof_main is actually SOF initialization */
	ret = sof_main(0, NULL);
	if (ret) {
		LOG_ERR("SOF initialization failed");
	}

	LOG_INF("SOF initialized");

#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
	/* Workaround for an apparent timing bug in libfuzzer+asan.
	 * If the initial/main thread is allowed to return, ASAN will
	 * fairly reliably report a "stack overflow" where the ESP and
	 * EPC (instruction pointer!) registers are both set to the
	 * same value, which is non-sensical.  See some discussion in
	 * https://github.com/zephyrproject-rtos/zephyr/pull/52769
	 *
	 * But suspending the main thread instead of aborting is cheap
	 * and easy.
	 */
	k_thread_suspend(k_current_get());
#endif
}

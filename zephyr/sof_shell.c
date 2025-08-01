// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
 */

#include <rtos/sof.h> /* sof_get() */
#include <sof/schedule/ll_schedule_domain.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>

#include <stdlib.h>

#define SOF_TEST_INJECT_SCHED_GAP_USEC 1500

static int cmd_sof_test_inject_sched_gap(const struct shell *sh,
		       size_t argc, char *argv[])
{
	uint32_t block_time = SOF_TEST_INJECT_SCHED_GAP_USEC;
	char *endptr = NULL;

#ifndef CONFIG_CROSS_CORE_STREAM
	shell_fprintf(sh, SHELL_NORMAL, "Domain blocking not supported, not reliable on SMP\n");
#endif

	domain_block(sof_get()->platform_timer_domain);

	if (argc > 1) {
		block_time = strtol(argv[1], &endptr, 0);
		if (endptr == argv[1])
			return -EINVAL;
	}

	k_busy_wait(block_time);

	domain_unblock(sof_get()->platform_timer_domain);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sof_commands,
	SHELL_CMD(test_inject_sched_gap, NULL,
		  "Inject a gap to audio scheduling\n",
		  cmd_sof_test_inject_sched_gap),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sof, &sof_commands,
		   "SOF application commands", NULL);

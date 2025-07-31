// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
 */

#include <rtos/sof.h> /* sof_get() */
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/audio/module_adapter/module/generic.h>

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

static int cmd_sof_module_heap_usage(const struct shell *sh,
				     size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	struct list_item *clist, *_clist;
	struct ipc_comp_dev *icd;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	list_for_item_safe(clist, _clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		shell_print(sh, "comp id 0x%08x\t%8zu bytes\t(%zu max)", icd->id,
			    module_adapter_heap_usage(comp_mod(icd->cd)),
			    comp_mod(icd->cd)->priv.cfg.heap_bytes);
	}
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sof_commands,
	SHELL_CMD(test_inject_sched_gap, NULL,
		  "Inject a gap to audio scheduling\n",
		  cmd_sof_test_inject_sched_gap),

	SHELL_CMD(module_heap_usage, NULL,
		  "Print heap memory usage of each module\n",
		  cmd_sof_module_heap_usage),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sof, &sof_commands,
		   "SOF application commands", NULL);

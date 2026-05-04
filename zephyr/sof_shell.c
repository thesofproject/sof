// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
 */

#include <rtos/sof.h> /* sof_get() */
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/topology.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <rtos/clk.h>
#include <rtos/alloc.h>
#if CONFIG_SOF_SHELL_MODULE_LIST
#include <rimage/sof/user/manifest.h>
#include <ipc4/base_fw_vendor.h>
#if CONFIG_LIBRARY_MANAGER
#include <sof/lib_manager.h>
#endif
#endif /* CONFIG_SOF_SHELL_MODULE_LIST */

#if CONFIG_SOF_SHELL_PIPELINE_OPS
#include <ipc4/pipeline.h>
#include <ipc4/module.h>
#include <ipc4/header.h>
#include <ctype.h>
#endif /* CONFIG_SOF_SHELL_PIPELINE_OPS */

#if CONFIG_SOF_SHELL_MMU_DBG
#include <zephyr/devicetree.h>
#include <zephyr/kernel/mm.h>
#include <zephyr/drivers/mm/system_mm.h>
#if CONFIG_XTENSA_MMU
#include <zephyr/arch/xtensa/xtensa_mmu.h>
#endif /* CONFIG_XTENSA_MMU */
#endif /* CONFIG_SOF_SHELL_MMU_DBG */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#if CONFIG_SYS_HEAP_RUNTIME_STATS
#include <zephyr/sys/sys_heap.h>
#endif

#include <stdlib.h>

#define SOF_TEST_INJECT_SCHED_GAP_USEC 1500

__cold static int cmd_sof_test_inject_sched_gap(const struct shell *sh,
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

#if CONFIG_SOF_SHELL_HEAP_USAGE
__cold static int cmd_sof_module_heap_usage(const struct shell *sh,
				     size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	struct list_item *clist, *_clist;
	struct ipc_comp_dev *icd;
	int count = 0;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	list_for_item_safe(clist, _clist, &ipc->comp_list) {
		size_t usage, hwm;

		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		usage = module_adapter_heap_usage(comp_mod(icd->cd), &hwm);
		shell_print(sh, "comp id 0x%08x%9zu usage%9zu hwm\tbytes",
			    icd->id, usage, hwm);
		count++;
	}

	if (!count)
		shell_print(sh, "No components found. Start an audio stream first.");

	return 0;
}
#endif /* CONFIG_SOF_SHELL_HEAP_USAGE */

#if CONFIG_SOF_SHELL_PIPELINE_STATUS || CONFIG_SOF_SHELL_MODULE_STATUS

__cold_rodata static const char * const comp_state_names[] = {
	[COMP_STATE_NOT_EXIST]  = "not_exist",
	[COMP_STATE_INIT]       = "init",
	[COMP_STATE_READY]      = "ready",
	[COMP_STATE_SUSPEND]    = "suspend",
	[COMP_STATE_PREPARE]    = "prepare",
	[COMP_STATE_PAUSED]     = "paused",
	[COMP_STATE_ACTIVE]     = "active",
	[COMP_STATE_PRE_ACTIVE] = "pre_active",
};

__cold static const char *comp_state_str(uint16_t state)
{
	if (state < ARRAY_SIZE(comp_state_names) && comp_state_names[state])
		return comp_state_names[state];
	return "unknown";
}

#endif /* CONFIG_SOF_SHELL_PIPELINE_STATUS || CONFIG_SOF_SHELL_MODULE_STATUS */

#if CONFIG_SOF_SHELL_PIPELINE_STATUS
__cold static int cmd_sof_pipeline_status(const struct shell *sh,
				   size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	struct list_item *clist;
	struct ipc_comp_dev *icd;
	int count = 0;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	shell_print(sh, "%-8s %-5s %-8s %-10s %-10s %s",
		    "ppl_id", "core", "priority", "period_us", "status", "state");

	list_for_item(clist, &ipc->comp_list) {
		struct pipeline *p;

		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_PIPELINE)
			continue;

		p = icd->pipeline;
		shell_print(sh, "%-8u %-5u %-8u %-10u %-10u %s",
			    p->pipeline_id, p->core, p->priority,
			    p->period, p->status,
			    comp_state_str((uint16_t)p->status));
		count++;
	}

	if (!count)
		shell_print(sh, "No pipelines found.");

	return 0;
}
#endif /* CONFIG_SOF_SHELL_PIPELINE_STATUS */

#if CONFIG_SOF_SHELL_MODULE_STATUS
__cold static int cmd_sof_module_status(const struct shell *sh,
				 size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	struct list_item *clist;
	struct ipc_comp_dev *icd;
	int count = 0;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	shell_print(sh, "%-12s %-8s %-5s %s",
		    "comp_id", "ppl_id", "core", "state");

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		shell_print(sh, "0x%-10x %-8u %-5u %s",
			    icd->id,
			    icd->cd->pipeline ? icd->cd->pipeline->pipeline_id : 0,
			    icd->core,
			    comp_state_str(icd->cd->state));
		count++;
	}

	if (!count)
		shell_print(sh, "No components found. Start an audio stream first.");

	return 0;
}
#endif /* CONFIG_SOF_SHELL_MODULE_STATUS */

#if CONFIG_SOF_SHELL_CORE_STATUS
__cold static int cmd_sof_core_status(const struct shell *sh,
			       size_t argc, char *argv[])
{
	int i;

	shell_print(sh, "%-6s %-8s %s", "core", "enabled", "current");

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		shell_print(sh, "%-6d %-8s %s",
			    i,
			    cpu_is_core_enabled(i) ? "yes" : "no",
			    (i == cpu_get_id()) ? "<--" : "");
	}

	return 0;
}
#endif /* CONFIG_SOF_SHELL_CORE_STATUS */

#if CONFIG_SOF_SHELL_SRAM_STATUS
__cold static int cmd_sof_sram_status(const struct shell *sh,
			       size_t argc, char *argv[])
{
	struct k_heap *h = sof_sys_heap_get();
	struct sys_memory_stats stats;

	if (!h) {
		shell_print(sh, "Heap not available");
		return 0;
	}

	sys_heap_runtime_stats_get(&h->heap, &stats);

	shell_print(sh, "HPSRAM heap (sof_heap):");
	shell_print(sh, "  allocated:    %zu bytes", stats.allocated_bytes);
	shell_print(sh, "  free:         %zu bytes", stats.free_bytes);
	shell_print(sh, "  max allocated:%zu bytes", stats.max_allocated_bytes);
	shell_print(sh, "  total:        %zu bytes",
		    stats.allocated_bytes + stats.free_bytes);

	return 0;
}
#endif /* CONFIG_SOF_SHELL_SRAM_STATUS */

#if CONFIG_SOF_SHELL_CLOCK_STATUS
__cold static int cmd_sof_clock_status(const struct shell *sh,
				size_t argc, char *argv[])
{
	struct clock_info *clocks = clocks_get();
	int i;

	if (!clocks) {
		shell_print(sh, "Clock info not available");
		return 0;
	}

	shell_print(sh, "%-6s %-12s %s", "clock", "freq_hz", "freq_mhz");

	for (i = 0; i < NUM_CLOCKS; i++) {
		uint32_t freq = clocks[i].freqs[clocks[i].current_freq_idx].freq;

		shell_print(sh, "%-6d %-12u %.1f",
			    i, freq, (double)freq / 1000000.0);
	}

	return 0;
}
#endif /* CONFIG_SOF_SHELL_CLOCK_STATUS */

SHELL_STATIC_SUBCMD_SET_CREATE(sof_test_commands,
	SHELL_CMD(ll_delay, NULL,
		  "Inject a scheduling gap to stress the LL timer domain\n",
		  cmd_sof_test_inject_sched_gap),
	SHELL_SUBCMD_SET_END
);

#if CONFIG_SOF_SHELL_HEAP_USAGE
SHELL_STATIC_SUBCMD_SET_CREATE(sof_mod_heap_commands,
	SHELL_CMD(usage, NULL,
		  "Print heap memory usage of each module\n",
		  cmd_sof_module_heap_usage),
	SHELL_SUBCMD_SET_END
);
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(sof_mod_commands,
#if CONFIG_SOF_SHELL_MODULE_STATUS
	SHELL_CMD(info, NULL,
		  "Print status of all active components\n",
		  cmd_sof_module_status),
#endif
#if CONFIG_SOF_SHELL_HEAP_USAGE
	SHELL_CMD(heap, &sof_mod_heap_commands,
		  "Heap usage commands\n",
		  NULL),
#endif
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_ppl_commands,
#if CONFIG_SOF_SHELL_PIPELINE_STATUS
	SHELL_CMD(list, NULL,
		  "Print status of all active pipelines\n",
		  cmd_sof_pipeline_status),
#endif
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_core_commands,
#if CONFIG_SOF_SHELL_CORE_STATUS
	SHELL_CMD(info, NULL,
		  "Print enabled/active state of each DSP core\n",
		  cmd_sof_core_status),
#endif
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_sram_commands,
#if CONFIG_SOF_SHELL_SRAM_STATUS
	SHELL_CMD(info, NULL,
		  "Print HPSRAM heap usage statistics\n",
		  cmd_sof_sram_status),
#endif
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_clock_commands,
#if CONFIG_SOF_SHELL_CLOCK_STATUS
	SHELL_CMD(info, NULL,
		  "Print current clock frequency for each DSP core\n",
		  cmd_sof_clock_status),
#endif
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_commands,
	SHELL_CMD(test, &sof_test_commands,
		  "Test commands: ll_delay\n",
		  NULL),

#if CONFIG_SOF_SHELL_MODULE_STATUS || CONFIG_SOF_SHELL_HEAP_USAGE
	SHELL_CMD(mod, &sof_mod_commands,
		  "Module commands: info, heap\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_PIPELINE_STATUS
	SHELL_CMD(ppl, &sof_ppl_commands,
		  "Pipeline commands: list\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_CORE_STATUS
	SHELL_CMD(core, &sof_core_commands,
		  "Core commands: info\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_SRAM_STATUS
	SHELL_CMD(sram, &sof_sram_commands,
		  "SRAM commands: info\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_CLOCK_STATUS
	SHELL_CMD(clock, &sof_clock_commands,
		  "Clock commands: info\n",
		  NULL),
#endif

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sof, &sof_commands,
		   "SOF application commands", NULL);

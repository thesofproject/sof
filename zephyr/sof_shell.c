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

#if CONFIG_SOF_SHELL_MODULE_LIST

/* Page size in DSP manifest entries (instance_bss_size, segment lengths) */
#ifdef CONFIG_MM_DRV_PAGE_SIZE
#define _SHELL_MOD_PAGE_SZ CONFIG_MM_DRV_PAGE_SIZE
#else
#define _SHELL_MOD_PAGE_SZ 4096
#endif

#if CONFIG_IPC4_BASE_FW_INTEL
__cold static void print_manifest_modules(const struct shell *sh,
					  const struct sof_man_fw_desc *desc,
					  int lib_id)
{
	const struct sof_man_mod_config *cfg_base;
	int i;

	if (!desc)
		return;

	cfg_base = (const struct sof_man_mod_config *)
		((const uint8_t *)desc +
		 SOF_MAN_MODULE_OFFSET(desc->header.num_module_entries));

	for (i = 0; i < (int)desc->header.num_module_entries; i++) {
		const struct sof_man_module *mod;
		const struct sof_man_mod_config *cfg = NULL;
		uint32_t text_sz, bss_sz;
		char name[SOF_MAN_MOD_NAME_LEN + 1];

		mod = (const struct sof_man_module *)
			((const uint8_t *)desc + SOF_MAN_MODULE_OFFSET(i));

		/* name is not null-terminated in the manifest */
		memcpy(name, mod->name, SOF_MAN_MOD_NAME_LEN);
		name[SOF_MAN_MOD_NAME_LEN] = '\0';

		if (mod->cfg_count > 0)
			cfg = cfg_base + mod->cfg_offset;

		text_sz = (uint32_t)mod->segment[0].flags.r.length * _SHELL_MOD_PAGE_SZ;
		bss_sz  = (uint32_t)mod->instance_bss_size * _SHELL_MOD_PAGE_SZ;

		shell_print(sh,
			    "[%d:%d] %-8s"
			    "  uuid:%08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x",
			    lib_id, i, name,
			    mod->uuid.a, mod->uuid.b, mod->uuid.c,
			    mod->uuid.d[0], mod->uuid.d[1],
			    mod->uuid.d[2], mod->uuid.d[3],
			    mod->uuid.d[4], mod->uuid.d[5],
			    mod->uuid.d[6], mod->uuid.d[7]);
		shell_print(sh,
			    "        inst_max:%-3u  bss/inst:%6u B  text:%6u B"
			    "  affinity:0x%02x",
			    mod->instance_max_count, bss_sz, text_sz,
			    mod->affinity_mask);
		if (cfg)
			shell_print(sh,
				    "        cpc:%-8u  cps:%-9u  ibs:%-6u  obs:%u",
				    cfg->cpc, cfg->cps, cfg->ibs, cfg->obs);
		else
			shell_print(sh, "        cpc:N/A");
	}
}
#endif /* CONFIG_IPC4_BASE_FW_INTEL */

__cold static int cmd_sof_module_list(const struct shell *sh,
				      size_t argc, char *argv[])
{
#if CONFIG_IPC4_BASE_FW_INTEL
	const struct sof_man_fw_desc *desc;
	int total = 0;

	shell_print(sh, "Built-in modules:");
	desc = basefw_vendor_get_manifest();
	if (desc) {
		print_manifest_modules(sh, desc, 0);
		total += (int)desc->header.num_module_entries;
	} else {
		shell_print(sh, "  (manifest not available)");
	}

#if CONFIG_LIBRARY_MANAGER
	{
		int lib_id;

		for (lib_id = 1; lib_id < LIB_MANAGER_MAX_LIBS; lib_id++) {
			desc = lib_manager_get_library_manifest(
					LIB_MANAGER_PACK_LIB_ID(lib_id));
			if (!desc)
				continue;
			shell_print(sh, "Library %d modules:", lib_id);
			print_manifest_modules(sh, desc, lib_id);
			total += (int)desc->header.num_module_entries;
		}
	}
#endif /* CONFIG_LIBRARY_MANAGER */

	if (!total)
		shell_print(sh, "No modules found.");

#else /* !CONFIG_IPC4_BASE_FW_INTEL */
	/* Generic fallback: list registered component drivers */
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;
	struct comp_driver_info *info;
	int count = 0;

	shell_print(sh, "%-5s  %-24s  %s", "type", "name", "uuid");

	list_for_item(clist, &drivers->list) {
		const struct sof_uuid *uid;
		const char *name;

		info = container_of(clist, struct comp_driver_info, list);
		uid = info->drv->uid;
		name = (info->drv->tctx && info->drv->tctx->uuid_p)
			? info->drv->tctx->uuid_p->name : "?";

		shell_print(sh,
			    "%-5u  %-24s"
			    "  %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x",
			    info->drv->type, name,
			    uid->a, uid->b, uid->c,
			    uid->d[0], uid->d[1], uid->d[2], uid->d[3],
			    uid->d[4], uid->d[5], uid->d[6], uid->d[7]);
		count++;
	}

	if (!count)
		shell_print(sh, "No drivers registered.");
#endif /* CONFIG_IPC4_BASE_FW_INTEL */

	return 0;
}
#endif /* CONFIG_SOF_SHELL_MODULE_LIST */

#if CONFIG_SOF_SHELL_PIPELINE_OPS

__cold static int parse_long(const struct shell *sh, const char *s, long *out,
			     long min_val, long max_val)
{
	char *endptr;
	long v = strtol(s, &endptr, 0);

	if (endptr == s || v < min_val || v > max_val) {
		shell_print(sh, "error: invalid value '%s' (allowed %ld..%ld)",
			    s, min_val, max_val);
		return -EINVAL;
	}
	*out = v;
	return 0;
}

/*
 * Resolve a module argument that may be either:
 *   - a numeric module_id  (e.g. "2", "0x02")
 *   - a module name string (e.g. "COPIER", "copier")  — IPC4/Intel only
 *
 * Returns 0 on success, -EINVAL on failure.
 */
__cold static int parse_module_id(const struct shell *sh, const char *s,
				  long *module_id)
{
	char *endptr;
	long v = strtol(s, &endptr, 0);

	/* Numeric: accepted if the whole string was consumed */
	if (endptr != s && *endptr == '\0') {
		if (v < 0 || v > 0xFFFF) {
			shell_print(sh, "error: module id 0x%lx out of range", v);
			return -EINVAL;
		}
		*module_id = v;
		return 0;
	}

#if CONFIG_IPC4_BASE_FW_INTEL
	/* Name lookup: search built-in manifest then loaded libraries */
	{
		char upper[SOF_MAN_MOD_NAME_LEN + 1];
		const struct sof_man_fw_desc *desc;
		uint32_t i;
		int k;

		/* Upper-case the input for case-insensitive compare */
		for (k = 0; k < SOF_MAN_MOD_NAME_LEN && s[k]; k++)
			upper[k] = (char)toupper((unsigned char)s[k]);
		upper[k] = '\0';

		desc = basefw_vendor_get_manifest();
		if (desc) {
			for (i = 0; i < desc->header.num_module_entries; i++) {
				const struct sof_man_module *mod =
					(const struct sof_man_module *)
					((const uint8_t *)desc +
					 SOF_MAN_MODULE_OFFSET(i));
				char mname[SOF_MAN_MOD_NAME_LEN + 1];
				int j;

				for (j = 0; j < SOF_MAN_MOD_NAME_LEN; j++)
					mname[j] = (char)toupper(
						(unsigned char)mod->name[j]);
				mname[SOF_MAN_MOD_NAME_LEN] = '\0';

				if (!strncmp(upper, mname,
					     SOF_MAN_MOD_NAME_LEN)) {
					*module_id = (long)mod->module_id;
					return 0;
				}
			}
		}

#if CONFIG_LIBRARY_MANAGER
		{
			int lib_id;

			for (lib_id = 1; lib_id < LIB_MANAGER_MAX_LIBS;
			     lib_id++) {
				uint32_t pack_id = LIB_MANAGER_PACK_LIB_ID(
					lib_id);

				desc = lib_manager_get_library_manifest(
					pack_id);
				if (!desc)
					continue;
				for (i = 0;
				     i < desc->header.num_module_entries;
				     i++) {
					const struct sof_man_module *mod =
						(const struct sof_man_module *)
						((const uint8_t *)desc +
						 SOF_MAN_MODULE_OFFSET(i));
					char mname[SOF_MAN_MOD_NAME_LEN + 1];
					int j;

					for (j = 0;
					     j < SOF_MAN_MOD_NAME_LEN; j++)
						mname[j] = (char)toupper(
							(unsigned char)
							mod->name[j]);
					mname[SOF_MAN_MOD_NAME_LEN] = '\0';

					if (!strncmp(upper, mname,
						     SOF_MAN_MOD_NAME_LEN)) {
						*module_id =
							(long)mod->module_id;
						return 0;
					}
				}
			}
		}
#endif /* CONFIG_LIBRARY_MANAGER */
	}
#endif /* CONFIG_IPC4_BASE_FW_INTEL */

	shell_print(sh, "error: unknown module '%s' (use name or numeric id)", s);
	return -EINVAL;
}

/* sof ppl_create <ppl_id> [priority=0] [pages=2] [core=0] [lp=0] */
__cold static int cmd_sof_ppl_create(const struct shell *sh,
				     size_t argc, char *argv[])
{
	struct ipc4_pipeline_create msg = {};
	struct ipc *ipc = sof_get()->ipc;
	long ppl_id, priority = 0, pages = 2, core = 0, lp = 0;
	int ret;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	if (parse_long(sh, argv[1], &ppl_id, 0, 255) < 0) return -EINVAL;
	if (argc > 2 && parse_long(sh, argv[2], &priority, 0, 7) < 0) return -EINVAL;
	if (argc > 3 && parse_long(sh, argv[3], &pages,    1, 2047) < 0) return -EINVAL;
	if (argc > 4 && parse_long(sh, argv[4], &core,     0, 7) < 0) return -EINVAL;
	if (argc > 5 && parse_long(sh, argv[5], &lp,       0, 1) < 0) return -EINVAL;

	msg.primary.r.ppl_mem_size = (uint32_t)pages;
	msg.primary.r.ppl_priority = (uint32_t)priority;
	msg.primary.r.instance_id  = (uint32_t)ppl_id;
	msg.primary.r.type         = SOF_IPC4_GLB_CREATE_PIPELINE;
	msg.extension.r.lp         = (uint32_t)lp;
	msg.extension.r.core_id    = (uint32_t)core;

	ret = ipc_pipeline_new(ipc, (ipc_pipe_new *)&msg);
	if (ret < 0)
		shell_print(sh, "ppl_create %ld failed: %d", ppl_id, ret);
	else
		shell_print(sh, "pipeline %ld created (prio=%ld pages=%ld core=%ld lp=%ld)",
			    ppl_id, priority, pages, core, lp);
	return 0;
}

/* sof ppl_delete <ppl_id> */
__cold static int cmd_sof_ppl_delete(const struct shell *sh,
				     size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	long ppl_id;
	int ret;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	if (parse_long(sh, argv[1], &ppl_id, 0, 255) < 0) return -EINVAL;

	ret = ipc_pipeline_free(ipc, (uint32_t)ppl_id);
	if (ret < 0)
		shell_print(sh, "ppl_delete %ld failed: %d", ppl_id, ret);
	else
		shell_print(sh, "pipeline %ld deleted", ppl_id);
	return 0;
}

/* sof ppl_state <ppl_id> <running|paused|reset> */
__cold static int cmd_sof_ppl_state(const struct shell *sh,
				    size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	struct ipc_comp_dev *ppl_icd;
	bool delayed = false;
	long ppl_id;
	uint32_t cmd;
	int ret;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	if (parse_long(sh, argv[1], &ppl_id, 0, 255) < 0) return -EINVAL;

	if (!strcmp(argv[2], "running"))
		cmd = SOF_IPC4_PIPELINE_STATE_RUNNING;
	else if (!strcmp(argv[2], "paused"))
		cmd = SOF_IPC4_PIPELINE_STATE_PAUSED;
	else if (!strcmp(argv[2], "reset"))
		cmd = SOF_IPC4_PIPELINE_STATE_RESET;
	else {
		shell_print(sh, "unknown state '%s' (running|paused|reset)", argv[2]);
		return -EINVAL;
	}

	ppl_icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
					 (uint32_t)ppl_id, IPC_COMP_IGNORE_REMOTE);
	if (!ppl_icd) {
		shell_print(sh, "pipeline %ld not found", ppl_id);
		return 0;
	}

	ret = ipc4_pipeline_prepare(ppl_icd, cmd);
	if (ret < 0) {
		shell_print(sh, "ppl_state %ld prepare failed: %d", ppl_id, ret);
		return 0;
	}

	ret = ipc4_pipeline_trigger(ppl_icd, cmd, &delayed);
	if (ret < 0)
		shell_print(sh, "ppl_state %ld trigger failed: %d", ppl_id, ret);
	else
		shell_print(sh, "pipeline %ld -> %s%s", ppl_id, argv[2],
			    delayed ? " (delayed)" : "");
	return 0;
}

/* sof mod_init <mod_name|mod_id> <inst_id> <ppl_id> [core=0] [dp=0] */
__cold static int cmd_sof_mod_init(const struct shell *sh,
				   size_t argc, char *argv[])
{
	struct ipc4_module_init_instance msg = {};
	struct comp_dev *dev;
	long mod_id, inst_id, ppl_id, core = 0, dp = 0;

	if (parse_module_id(sh, argv[1], &mod_id) < 0) return -EINVAL;
	if (parse_long(sh, argv[2], &inst_id, 0, 255)    < 0) return -EINVAL;
	if (parse_long(sh, argv[3], &ppl_id,  0, 255)    < 0) return -EINVAL;
	if (argc > 4 && parse_long(sh, argv[4], &core, 0, 7) < 0) return -EINVAL;
	if (argc > 5 && parse_long(sh, argv[5], &dp,   0, 1) < 0) return -EINVAL;

	msg.primary.r.module_id   = (uint32_t)mod_id;
	msg.primary.r.instance_id = (uint32_t)inst_id;
	msg.primary.r.type        = SOF_IPC4_MOD_INIT_INSTANCE;
	msg.primary.r.msg_tgt     = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	msg.extension.r.ppl_instance_id = (uint32_t)ppl_id;
	msg.extension.r.core_id         = (uint32_t)core;
	msg.extension.r.proc_domain     = (uint32_t)dp;
	msg.extension.r.param_block_size = 0;

	dev = comp_new_ipc4(&msg);
	if (!dev)
		shell_print(sh, "mod_init module=0x%lx inst=%ld failed",
			    mod_id, inst_id);
	else
		shell_print(sh,
			    "module 0x%lx inst %ld created in pipeline %ld"
			    " comp_id=0x%08x",
			    mod_id, inst_id, ppl_id,
			    IPC4_COMP_ID((uint32_t)mod_id, (uint32_t)inst_id));
	return 0;
}

/* sof mod_delete <mod_name|mod_id> <inst_id> */
__cold static int cmd_sof_mod_delete(const struct shell *sh,
				     size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	long mod_id, inst_id;
	uint32_t comp_id;
	int ret;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	if (parse_module_id(sh, argv[1], &mod_id) < 0) return -EINVAL;
	if (parse_long(sh, argv[2], &inst_id, 0, 255)    < 0) return -EINVAL;

	comp_id = IPC4_COMP_ID((uint32_t)mod_id, (uint32_t)inst_id);
	ret = ipc_comp_free(ipc, comp_id);
	if (ret < 0)
		shell_print(sh, "mod_delete module=0x%lx inst=%ld failed: %d",
			    mod_id, inst_id, ret);
	else
		shell_print(sh, "module 0x%lx inst %ld deleted", mod_id, inst_id);
	return 0;
}

/* sof mod_bind <src_name|src_id> <src_inst> <dst_name|dst_id> <dst_inst> [sq=0] [dq=0] */
__cold static int cmd_sof_mod_bind(const struct shell *sh,
				   size_t argc, char *argv[])
{
	struct ipc4_module_bind_unbind msg = {};
	struct ipc *ipc = sof_get()->ipc;
	long src_mod, src_inst, dst_mod, dst_inst, src_q = 0, dst_q = 0;
	int ret;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	if (parse_module_id(sh, argv[1], &src_mod) < 0) return -EINVAL;
	if (parse_long(sh, argv[2], &src_inst, 0, 255)    < 0) return -EINVAL;
	if (parse_module_id(sh, argv[3], &dst_mod) < 0) return -EINVAL;
	if (parse_long(sh, argv[4], &dst_inst, 0, 255)    < 0) return -EINVAL;
	if (argc > 5 && parse_long(sh, argv[5], &src_q, 0, 7) < 0) return -EINVAL;
	if (argc > 6 && parse_long(sh, argv[6], &dst_q, 0, 7) < 0) return -EINVAL;

	msg.primary.r.module_id   = (uint32_t)src_mod;
	msg.primary.r.instance_id = (uint32_t)src_inst;
	msg.primary.r.type        = SOF_IPC4_MOD_BIND;
	msg.primary.r.msg_tgt     = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	msg.extension.r.dst_module_id   = (uint32_t)dst_mod;
	msg.extension.r.dst_instance_id = (uint32_t)dst_inst;
	msg.extension.r.src_queue       = (uint32_t)src_q;
	msg.extension.r.dst_queue       = (uint32_t)dst_q;

	ret = ipc_comp_connect(ipc, (ipc_pipe_comp_connect *)&msg);
	if (ret < 0)
		shell_print(sh, "mod_bind failed: %d", ret);
	else
		shell_print(sh, "bound 0x%lx:%ld[q%ld] -> 0x%lx:%ld[q%ld]",
			    src_mod, src_inst, src_q,
			    dst_mod, dst_inst, dst_q);
	return 0;
}

/* sof mod_unbind <src_name|src_id> <src_inst> <dst_name|dst_id> <dst_inst> [sq=0] [dq=0] */
__cold static int cmd_sof_mod_unbind(const struct shell *sh,
				     size_t argc, char *argv[])
{
	struct ipc4_module_bind_unbind msg = {};
	struct ipc *ipc = sof_get()->ipc;
	long src_mod, src_inst, dst_mod, dst_inst, src_q = 0, dst_q = 0;
	int ret;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	if (parse_module_id(sh, argv[1], &src_mod) < 0) return -EINVAL;
	if (parse_long(sh, argv[2], &src_inst, 0, 255)    < 0) return -EINVAL;
	if (parse_module_id(sh, argv[3], &dst_mod) < 0) return -EINVAL;
	if (parse_long(sh, argv[4], &dst_inst, 0, 255)    < 0) return -EINVAL;
	if (argc > 5 && parse_long(sh, argv[5], &src_q, 0, 7) < 0) return -EINVAL;
	if (argc > 6 && parse_long(sh, argv[6], &dst_q, 0, 7) < 0) return -EINVAL;

	msg.primary.r.module_id   = (uint32_t)src_mod;
	msg.primary.r.instance_id = (uint32_t)src_inst;
	msg.primary.r.type        = SOF_IPC4_MOD_UNBIND;
	msg.primary.r.msg_tgt     = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	msg.extension.r.dst_module_id   = (uint32_t)dst_mod;
	msg.extension.r.dst_instance_id = (uint32_t)dst_inst;
	msg.extension.r.src_queue       = (uint32_t)src_q;
	msg.extension.r.dst_queue       = (uint32_t)dst_q;

	ret = ipc_comp_disconnect(ipc, (ipc_pipe_comp_connect *)&msg);
	if (ret < 0)
		shell_print(sh, "mod_unbind failed: %d", ret);
	else
		shell_print(sh, "unbound 0x%lx:%ld[q%ld] -/- 0x%lx:%ld[q%ld]",
			    src_mod, src_inst, src_q,
			    dst_mod, dst_inst, dst_q);
	return 0;
}

#endif /* CONFIG_SOF_SHELL_PIPELINE_OPS */

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
#if CONFIG_SOF_SHELL_MODULE_LIST
	SHELL_CMD(list, NULL,
		  "List all available modules with name, memory, size and RTC info\n",
		  cmd_sof_module_list),
#endif
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
#if CONFIG_SOF_SHELL_PIPELINE_OPS
	SHELL_CMD_ARG(init, NULL,
		  "Instantiate module: <name|id> <inst_id> <ppl_id> [core=0] [dp=0]\n",
		  cmd_sof_mod_init, 4, 2),
	SHELL_CMD_ARG(delete, NULL,
		  "Delete module instance: <name|id> <inst_id>\n",
		  cmd_sof_mod_delete, 3, 0),
	SHELL_CMD_ARG(bind, NULL,
		  "Bind two module instances: <sname|sid> <si> <dname|did> <di>"
		  " [sq=0] [dq=0]\n",
		  cmd_sof_mod_bind, 5, 2),
	SHELL_CMD_ARG(unbind, NULL,
		  "Unbind two module instances: <sname|sid> <si> <dname|did> <di>"
		  " [sq=0] [dq=0]\n",
		  cmd_sof_mod_unbind, 5, 2),
#endif
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_ppl_commands,
#if CONFIG_SOF_SHELL_PIPELINE_STATUS
	SHELL_CMD(list, NULL,
		  "Print status of all active pipelines\n",
		  cmd_sof_pipeline_status),
#endif
#if CONFIG_SOF_SHELL_PIPELINE_OPS
	SHELL_CMD_ARG(create, NULL,
		  "Create IPC4 pipeline: <ppl_id> [priority=0] [pages=2] [core=0] [lp=0]\n",
		  cmd_sof_ppl_create, 2, 4),
	SHELL_CMD_ARG(delete, NULL,
		  "Delete IPC4 pipeline: <ppl_id>\n",
		  cmd_sof_ppl_delete, 2, 0),
	SHELL_CMD_ARG(state, NULL,
		  "Set IPC4 pipeline state: <ppl_id> <running|paused|reset>\n",
		  cmd_sof_ppl_state, 3, 0),
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

#if CONFIG_SOF_SHELL_MODULE_LIST || CONFIG_SOF_SHELL_MODULE_STATUS || CONFIG_SOF_SHELL_HEAP_USAGE || CONFIG_SOF_SHELL_PIPELINE_OPS
	SHELL_CMD(mod, &sof_mod_commands,
		  "Module commands: list, info, heap, init, delete, bind, unbind\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_PIPELINE_STATUS || CONFIG_SOF_SHELL_PIPELINE_OPS
	SHELL_CMD(ppl, &sof_ppl_commands,
		  "Pipeline commands: list, create, delete, state\n",
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

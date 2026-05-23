// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
 */

#include <rtos/sof.h> /* sof_get() */
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/buffer.h>
#include <sof/audio/audio_stream.h>
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

#if CONFIG_SOF_SHELL_LLEXT_LOAD
#include <sof/shell_llext_load.h>
#include <sof/lib_manager.h>
#include <adsp_debug_window.h>
#endif /* CONFIG_SOF_SHELL_LLEXT_LOAD */

#if (CONFIG_SOF_SHELL_LLEXT_LIST || CONFIG_SOF_SHELL_LLEXT_PURGE) && CONFIG_LLEXT
#include <zephyr/llext/llext.h>
#endif

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#if CONFIG_SYS_HEAP_RUNTIME_STATS
#include <zephyr/sys/sys_heap.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sof/ipc/common.h>

#define SOF_TEST_INJECT_SCHED_GAP_USEC 1500

#include <sof_versions.h>
#include <zephyr/version.h>
#include <sof/lib/vpage.h>
#include <sof/lib/vregion.h>

#if CONFIG_SOF_SHELL_PIPELINE_OPS
__cold static const char *module_name_from_id(uint32_t module_id,
					      char *name_buf);
#endif

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
		uint32_t mod_id;
		const char *mod_name = "unknown";
#if CONFIG_SOF_SHELL_PIPELINE_OPS
		char mod_name_buf[SOF_MAN_MOD_NAME_LEN + 1];
#endif

		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		mod_id = icd->id & 0xffff;

#if CONFIG_SOF_SHELL_PIPELINE_OPS
		mod_name = module_name_from_id(mod_id, mod_name_buf);
#endif

		usage = module_adapter_heap_usage(comp_mod(icd->cd), &hwm);
		shell_print(sh,
			    "comp_id=0x%08x module=%s(0x%04x) usage=%zu hwm=%zu bytes",
			    icd->id, mod_name, mod_id, usage, hwm);
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

	shell_print(sh, "%-12s %-26s %-8s %-5s %s",
		    "comp_id", "module", "ppl_id", "core", "state");

	list_for_item(clist, &ipc->comp_list) {
		uint32_t mod_id;
		const char *mod_name = "unknown";
#if CONFIG_SOF_SHELL_PIPELINE_OPS
		char mod_name_buf[SOF_MAN_MOD_NAME_LEN + 1];
#endif

		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		mod_id = icd->id & 0xffff;
#if CONFIG_SOF_SHELL_PIPELINE_OPS
		mod_name = module_name_from_id(mod_id, mod_name_buf);
#endif

		shell_print(sh, "0x%-10x %-18s(0x%04x) %-8u %-5u %s",
			    icd->id,
			    mod_name,
			    mod_id,
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

#if CONFIG_SOF_SHELL_CORE_POWER

/*
 * sof core_on <core_id>
 * sof core_off <core_id>
 *
 * Power a secondary DSP core on or off.  Core 0 (primary) cannot be
 * controlled via these commands.
 */
__cold static int cmd_sof_core_on(const struct shell *sh,
				  size_t argc, char *argv[])
{
	char *endptr = NULL;
	long id;
	int ret;

	id = strtol(argv[1], &endptr, 0);
	if (endptr == argv[1] || id < 1 || id >= CONFIG_CORE_COUNT) {
		shell_error(sh, "core_id must be 1..%d", CONFIG_CORE_COUNT - 1);
		return -EINVAL;
	}

	if (cpu_is_core_enabled((int)id)) {
		shell_print(sh, "core %ld already active", id);
		return 0;
	}

	ret = cpu_enable_core((int)id);
	if (ret)
		shell_error(sh, "core_on: failed to enable core %ld: %d", id, ret);
	else
		shell_print(sh, "core_on: core %ld enabled", id);

	return ret;
}

__cold static int cmd_sof_core_off(const struct shell *sh,
				   size_t argc, char *argv[])
{
	char *endptr = NULL;
	long id;

	id = strtol(argv[1], &endptr, 0);
	if (endptr == argv[1] || id < 1 || id >= CONFIG_CORE_COUNT) {
		shell_error(sh, "core_id must be 1..%d", CONFIG_CORE_COUNT - 1);
		return -EINVAL;
	}

	if (!cpu_is_core_enabled((int)id)) {
		shell_print(sh, "core %ld already inactive", id);
		return 0;
	}

	cpu_disable_core((int)id);

	if (cpu_is_core_enabled((int)id)) {
		shell_error(sh, "core_off: core %ld did not power down", id);
		return -EIO;
	}

	shell_print(sh, "core_off: core %ld disabled", id);
	return 0;
}

#endif /* CONFIG_SOF_SHELL_CORE_POWER */

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
		uint32_t mhz_whole = freq / 1000000;
		uint32_t mhz_tenth = (freq % 1000000) / 100000;

		shell_print(sh, "%-6d %-12u %u.%u",
			    i, freq, mhz_whole, mhz_tenth);
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
		uint32_t text_sz = 0, bss_sz;
		char name[SOF_MAN_MOD_NAME_LEN + 1];
		int s;

		mod = (const struct sof_man_module *)
			((const uint8_t *)desc + SOF_MAN_MODULE_OFFSET(i));

		/* name is not null-terminated in the manifest */
		memcpy(name, mod->name, SOF_MAN_MOD_NAME_LEN);
		name[SOF_MAN_MOD_NAME_LEN] = '\0';

		if (mod->cfg_count > 0)
			cfg = cfg_base + mod->cfg_offset;

		/* find TEXT segment by type — index 0 is not always TEXT for built-ins */
		for (s = 0; s < 3; s++) {
			if (mod->segment[s].flags.r.type == SOF_MAN_SEGMENT_TEXT) {
				text_sz = (uint32_t)mod->segment[s].flags.r.length
					  * _SHELL_MOD_PAGE_SZ;
				break;
			}
		}
		bss_sz = (uint32_t)mod->instance_bss_size * _SHELL_MOD_PAGE_SZ;

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

		/* yield between entries so the serial driver can drain its TX FIFO */
		k_msleep(5);
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

/* parse_long: used by PIPELINE_OPS commands and LLEXT_LOAD; must be outside
 * individual feature guards so all callers can see it.
 */
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

#if CONFIG_SOF_SHELL_PIPELINE_OPS

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

__cold static const char *module_name_from_id(uint32_t module_id,
					      char *name_buf)
{
#if CONFIG_IPC4_BASE_FW_INTEL
	const struct sof_man_fw_desc *desc;
	uint32_t i;

	desc = basefw_vendor_get_manifest();
	if (desc) {
		for (i = 0; i < desc->header.num_module_entries; i++) {
			const struct sof_man_module *mod =
				(const struct sof_man_module *)
				((const uint8_t *)desc + SOF_MAN_MODULE_OFFSET(i));

			if (mod->module_id == module_id) {
				memcpy(name_buf, mod->name, SOF_MAN_MOD_NAME_LEN);
				name_buf[SOF_MAN_MOD_NAME_LEN] = '\0';
				return name_buf;
			}
		}
	}

#if CONFIG_LIBRARY_MANAGER
	{
		int lib_id;

		for (lib_id = 1; lib_id < LIB_MANAGER_MAX_LIBS; lib_id++) {
			desc = lib_manager_get_library_manifest(
				LIB_MANAGER_PACK_LIB_ID(lib_id));
			if (!desc)
				continue;

			for (i = 0; i < desc->header.num_module_entries; i++) {
				const struct sof_man_module *mod =
					(const struct sof_man_module *)
					((const uint8_t *)desc + SOF_MAN_MODULE_OFFSET(i));

				if (mod->module_id == module_id) {
					memcpy(name_buf, mod->name,
					       SOF_MAN_MOD_NAME_LEN);
					name_buf[SOF_MAN_MOD_NAME_LEN] = '\0';
					return name_buf;
				}
			}
		}
	}
#endif /* CONFIG_LIBRARY_MANAGER */
#endif /* CONFIG_IPC4_BASE_FW_INTEL */

	return "unknown";
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
	char mod_name[SOF_MAN_MOD_NAME_LEN + 1];
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
		shell_print(sh, "mod_init module=%s(0x%lx) inst=%ld failed",
			    module_name_from_id((uint32_t)mod_id, mod_name),
			    mod_id, inst_id);
	else
		shell_print(sh,
			    "module %s(0x%lx) inst %ld created in pipeline %ld"
			    " comp_id=0x%08x",
			    module_name_from_id((uint32_t)mod_id, mod_name),
			    mod_id, inst_id, ppl_id,
			    IPC4_COMP_ID((uint32_t)mod_id, (uint32_t)inst_id));
	return 0;
}

/* sof mod_delete <mod_name|mod_id> <inst_id> */
__cold static int cmd_sof_mod_delete(const struct shell *sh,
				     size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	char mod_name[SOF_MAN_MOD_NAME_LEN + 1];
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
		shell_print(sh, "mod_delete module=%s(0x%lx) inst=%ld failed: %d",
			    module_name_from_id((uint32_t)mod_id, mod_name),
			    mod_id, inst_id, ret);
	else
		shell_print(sh, "module %s(0x%lx) inst %ld deleted",
			    module_name_from_id((uint32_t)mod_id, mod_name),
			    mod_id, inst_id);
	return 0;
}

/* sof mod_bind <src_name|src_id> <src_inst> <dst_name|dst_id> <dst_inst> [sq=0] [dq=0] */
__cold static int cmd_sof_mod_bind(const struct shell *sh,
				   size_t argc, char *argv[])
{
	struct ipc4_module_bind_unbind msg = {};
	struct ipc *ipc = sof_get()->ipc;
	char src_name[SOF_MAN_MOD_NAME_LEN + 1];
	char dst_name[SOF_MAN_MOD_NAME_LEN + 1];
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
		shell_print(sh,
			    "mod_bind %s(0x%lx):%ld[q%ld] -> %s(0x%lx):%ld[q%ld] failed: %d",
			    module_name_from_id((uint32_t)src_mod, src_name),
			    src_mod, src_inst, src_q,
			    module_name_from_id((uint32_t)dst_mod, dst_name),
			    dst_mod, dst_inst, dst_q,
			    ret);
	else
		shell_print(sh, "bound %s(0x%lx):%ld[q%ld] -> %s(0x%lx):%ld[q%ld]",
			    module_name_from_id((uint32_t)src_mod, src_name),
			    src_mod, src_inst, src_q,
			    module_name_from_id((uint32_t)dst_mod, dst_name),
			    dst_mod, dst_inst, dst_q);
	return 0;
}

/* sof mod_unbind <src_name|src_id> <src_inst> <dst_name|dst_id> <dst_inst> [sq=0] [dq=0] */
__cold static int cmd_sof_mod_unbind(const struct shell *sh,
				     size_t argc, char *argv[])
{
	struct ipc4_module_bind_unbind msg = {};
	struct ipc *ipc = sof_get()->ipc;
	char src_name[SOF_MAN_MOD_NAME_LEN + 1];
	char dst_name[SOF_MAN_MOD_NAME_LEN + 1];
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
		shell_print(sh,
			    "mod_unbind %s(0x%lx):%ld[q%ld] -/- %s(0x%lx):%ld[q%ld] failed: %d",
			    module_name_from_id((uint32_t)src_mod, src_name),
			    src_mod, src_inst, src_q,
			    module_name_from_id((uint32_t)dst_mod, dst_name),
			    dst_mod, dst_inst, dst_q,
			    ret);
	else
		shell_print(sh, "unbound %s(0x%lx):%ld[q%ld] -/- %s(0x%lx):%ld[q%ld]",
			    module_name_from_id((uint32_t)src_mod, src_name),
			    src_mod, src_inst, src_q,
			    module_name_from_id((uint32_t)dst_mod, dst_name),
			    dst_mod, dst_inst, dst_q);
	return 0;
}

#endif /* CONFIG_SOF_SHELL_PIPELINE_OPS */

#if CONFIG_SOF_SHELL_MMU_DBG

#if CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB

/*
 * Lightweight wrappers around the Intel ADSP MTL TLB MMIO table.
 * Mirrors mm_drv_intel_adsp.h without pulling in the driver-internal header.
 */
#define _SHELL_TLB_NODE       DT_NODELABEL(tlb)
#define _SHELL_TLB_BASE       ((volatile uint16_t *)(uintptr_t)DT_REG_ADDR(_SHELL_TLB_NODE))
#define _SHELL_PADDR_SIZE     DT_PROP(_SHELL_TLB_NODE, paddr_size)
#define _SHELL_TLB_ENTRY_NUM  BIT(_SHELL_PADDR_SIZE)
#define _SHELL_PADDR_MASK     (_SHELL_TLB_ENTRY_NUM - 1)
#define _SHELL_ENABLE_BIT     ((uint16_t)BIT(_SHELL_PADDR_SIZE))
#define _SHELL_EXEC_BIT       ((uint16_t)BIT(DT_PROP(_SHELL_TLB_NODE, exec_bit_idx)))
#define _SHELL_WRITE_BIT      ((uint16_t)BIT(DT_PROP(_SHELL_TLB_NODE, write_bit_idx)))

/*
 * Base physical address for the HPSRAM region (mirrors TLB_PHYS_BASE in the
 * driver).  Physical pages whose index fits in paddr_size bits are located
 * starting here.
 */
#define _SHELL_PHYS_BASE \
	(((CONFIG_KERNEL_VM_BASE / CONFIG_MM_DRV_PAGE_SIZE) & ~_SHELL_PADDR_MASK) * \
	 CONFIG_MM_DRV_PAGE_SIZE)

/* Convert virtual-address index → physical address */
__cold static uintptr_t shell_tlb_idx_to_pa(uint32_t idx, uint16_t entry)
{
	return _SHELL_PHYS_BASE +
	       ((uintptr_t)(entry & _SHELL_PADDR_MASK) * CONFIG_MM_DRV_PAGE_SIZE);
}

/* Decode 16-bit TLB entry permission bits into a short string */
__cold static void shell_tlb_flags_str(uint16_t entry, char *buf)
{
	buf[0] = 'R';
	buf[1] = (entry & _SHELL_WRITE_BIT) ? 'W' : '-';
	buf[2] = (entry & _SHELL_EXEC_BIT)  ? 'X' : '-';
	buf[3] = '\0';
}

/* sof mmu_status */
__cold static int cmd_sof_mmu_status(const struct shell *sh,
				     size_t argc, char *argv[])
{
	const struct sys_mm_drv_region *regions, *r;
	volatile uint16_t *tlb = _SHELL_TLB_BASE;
	uint32_t total = _SHELL_TLB_ENTRY_NUM;
	uint32_t enabled = 0;
	uint32_t i;

	/* Count active TLB entries */
	for (i = 0; i < total; i++) {
		if (tlb[i] & _SHELL_ENABLE_BIT)
			enabled++;
	}

	shell_print(sh, "Intel ADSP MTL TLB / Virtual Memory Status");
	shell_print(sh, "  VM base:       0x%08x", CONFIG_KERNEL_VM_BASE);
	shell_print(sh, "  VM size:       0x%08x (%u KB)",
		    (uint32_t)(total * CONFIG_MM_DRV_PAGE_SIZE),
		    (uint32_t)(total * CONFIG_MM_DRV_PAGE_SIZE / 1024));
	shell_print(sh, "  page size:     %u B", CONFIG_MM_DRV_PAGE_SIZE);
	shell_print(sh, "  total entries: %u", total);
	shell_print(sh, "  mapped pages:  %u (%u KB)",
		    enabled, enabled * CONFIG_MM_DRV_PAGE_SIZE / 1024);
	shell_print(sh, "  free pages:    %u (%u KB)",
		    total - enabled,
		    (total - enabled) * CONFIG_MM_DRV_PAGE_SIZE / 1024);
	shell_print(sh, "  TLB MMIO base: 0x%08x",
		    (uint32_t)(uintptr_t)_SHELL_TLB_BASE);
	shell_print(sh, "  paddr_size:    %u  enable_bit:%u  exec_bit:%u  write_bit:%u",
		    _SHELL_PADDR_SIZE,
		    _SHELL_PADDR_SIZE,
		    DT_PROP(_SHELL_TLB_NODE, exec_bit_idx),
		    DT_PROP(_SHELL_TLB_NODE, write_bit_idx));

	shell_print(sh, "");
	shell_print(sh, "Mapped memory regions (sys_mm_drv):");
	shell_print(sh, "  %-10s  %-10s  %s", "address", "size", "attr");

	regions = sys_mm_drv_query_memory_regions();
	if (regions) {
		SYS_MM_DRV_MEMORY_REGION_FOREACH(regions, r) {
			shell_print(sh, "  0x%08x  0x%08x  0x%08x",
				    (uint32_t)(uintptr_t)r->addr,
				    (uint32_t)r->size,
				    (uint32_t)r->attr);
		}
		sys_mm_drv_query_memory_regions_free(regions);
	} else {
		shell_print(sh, "  (not available)");
	}
	return 0;
}

/* sof tlb_dump */
__cold static int cmd_sof_tlb_dump(const struct shell *sh,
				   size_t argc, char *argv[])
{
	volatile uint16_t *tlb = _SHELL_TLB_BASE;
	uint32_t total = _SHELL_TLB_ENTRY_NUM;
	uint32_t count = 0;
	uint32_t i;

	shell_print(sh, "  idx   vaddr       paddr       flags  entry");

	for (i = 0; i < total; i++) {
		uint16_t entry = tlb[i];

		if (!(entry & _SHELL_ENABLE_BIT))
			continue;

		uintptr_t vaddr = CONFIG_KERNEL_VM_BASE +
				  (uintptr_t)i * CONFIG_MM_DRV_PAGE_SIZE;
		uintptr_t paddr = shell_tlb_idx_to_pa(i, entry);
		char flags[4];

		shell_tlb_flags_str(entry, flags);
		shell_print(sh, "  %-5u 0x%08x  0x%08x  %s    0x%04x",
			    i, (uint32_t)vaddr, (uint32_t)paddr,
			    flags, (uint32_t)entry);
		count++;
	}

	shell_print(sh, "Total: %u/%u entries active", count, total);
	return 0;
}

/* sof tlb_lookup <vaddr> [end_vaddr] */
__cold static int cmd_sof_tlb_lookup(const struct shell *sh,
				     size_t argc, char *argv[])
{
	volatile uint16_t *tlb = _SHELL_TLB_BASE;
	uintptr_t vstart, vend;

	/* Parse start address */
	{
		char *ep;
		unsigned long v = strtoul(argv[1], &ep, 0);

		if (ep == argv[1]) {
			shell_print(sh, "error: invalid address '%s'", argv[1]);
			return -EINVAL;
		}
		vstart = (uintptr_t)v & ~(CONFIG_MM_DRV_PAGE_SIZE - 1);
	}

	if (argc > 2) {
		char *ep;
		unsigned long v = strtoul(argv[2], &ep, 0);

		if (ep == argv[2]) {
			shell_print(sh, "error: invalid address '%s'", argv[2]);
			return -EINVAL;
		}
		vend = (uintptr_t)v & ~(CONFIG_MM_DRV_PAGE_SIZE - 1);
	} else {
		vend = vstart;
	}

	if (vend < vstart)
		vend = vstart;

	shell_print(sh, "  vaddr       paddr       mapped  flags  bank  entry");

	for (uintptr_t va = vstart; va <= vend; va += CONFIG_MM_DRV_PAGE_SIZE) {
		uintptr_t vm_base = CONFIG_KERNEL_VM_BASE;
		uintptr_t vm_end  = vm_base +
				    (uintptr_t)_SHELL_TLB_ENTRY_NUM *
				    CONFIG_MM_DRV_PAGE_SIZE - 1;

		if (va < vm_base || va > vm_end) {
			shell_print(sh, "  0x%08x  (outside VM range)",
				    (uint32_t)va);
			continue;
		}

		uint32_t idx = (uint32_t)((va - vm_base) /
					  CONFIG_MM_DRV_PAGE_SIZE);
		uint16_t entry = tlb[idx];
		bool mapped = (entry & _SHELL_ENABLE_BIT) != 0;

		if (!mapped) {
			shell_print(sh, "  0x%08x  (not mapped)",
				    (uint32_t)va);
			continue;
		}

		uintptr_t pa = shell_tlb_idx_to_pa(idx, entry);
		uint32_t bank = (uint32_t)((pa - _SHELL_PHYS_BASE) /
					   (128 * 1024));
		char flags[4];

		shell_tlb_flags_str(entry, flags);
		shell_print(sh, "  0x%08x  0x%08x  yes     %s    %-4u  0x%04x",
			    (uint32_t)va, (uint32_t)pa,
			    flags, bank, (uint32_t)entry);
	}
	return 0;
}

#endif /* CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB */

#if CONFIG_XTENSA_MMU

/*
 * Xtensa hardware MMU helpers.
 *
 * PDTLB result layout (Xtensa ISA §4.6.5):
 *   bit[4]   = HIT
 *   bits[3:0] = TLB way (valid when HIT)
 *
 * RDTLB0 result:
 *   bits[31:12] = Virtual Page Number
 *   bits[5:4]   = ring (0=kernel, 1=unused, 2=user, 3=shared)
 *   bits[3:0]   = CA (cache + access attributes)
 *
 * RDTLB1 result:
 *   bits[31:12] = Physical Page Number
 *   bits[3:0]   = CA (same as RDTLB0)
 *
 * CA bits (from <zephyr/arch/xtensa/xtensa_mmu.h>):
 *   bit 0 = XTENSA_MMU_PERM_X   (executable)
 *   bit 1 = XTENSA_MMU_PERM_W   (writable)
 *   bit 2 = XTENSA_MMU_CACHED_WB (write-back cache)
 *   bit 3 = XTENSA_MMU_CACHED_WT (write-through cache)
 *   bits 2+3 both set = illegal / not-present
 */
#define SHELL_PDTLB_HIT    0x10U  /* bit 4 of pdtlb result */
#define SHELL_PTE_RING_SHIFT 4U
#define SHELL_PTE_RING_MASK  0x30U
#define SHELL_PTE_CA_MASK    0x0FU
#define SHELL_PTE_PPN_MASK   0xFFFFF000U

static inline uint32_t shell_pdtlb(void *vaddr)
{
	uint32_t r;
	__asm__ __volatile__("pdtlb %0, %1\n\t" : "=a"(r) : "a"((uint32_t)vaddr));
	return r;
}

static inline uint32_t shell_rdtlb0(uint32_t entry)
{
	uint32_t r;
	__asm__ volatile("rdtlb0 %0, %1\n\t" : "=a"(r) : "a"(entry));
	return r;
}

static inline uint32_t shell_rdtlb1(uint32_t entry)
{
	uint32_t r;
	__asm__ volatile("rdtlb1 %0, %1\n\t" : "=a"(r) : "a"(entry));
	return r;
}

static inline uint32_t shell_rsr_rasid(void)
{
	uint32_t r;
	__asm__ volatile("rsr %0, rasid" : "=a"(r));
	return r;
}

/* Decode 4-bit CA cache field into a short string */
__cold static const char *ca_cache_str(uint32_t ca)
{
	switch (ca & (XTENSA_MMU_CACHED_WB | XTENSA_MMU_CACHED_WT)) {
	case 0:                     return "uncached";
	case XTENSA_MMU_CACHED_WB:  return "WB      ";
	case XTENSA_MMU_CACHED_WT:  return "WT      ";
	default:                    return "illegal ";
	}
}

__cold_rodata static const char * const ring_name[] = {
	"kernel", "unused", "user", "shared"
};

/* sof rasid */
__cold static int cmd_sof_rasid(const struct shell *sh,
				size_t argc, char *argv[])
{
	uint32_t rasid = shell_rsr_rasid();
	int ring;

	shell_print(sh, "RASID: 0x%08x", rasid);
	for (ring = 0; ring < 4; ring++) {
		uint8_t asid = (uint8_t)((rasid >> (ring * 8)) & 0xff);

		shell_print(sh, "  ring %d (%s):\tASID 0x%02x",
			    ring, ring_name[ring], asid);
	}
	return 0;
}

/* sof page_info <vaddr> [end_vaddr] */
__cold static int cmd_sof_page_info(const struct shell *sh,
				    size_t argc, char *argv[])
{
	uintptr_t vstart, vend;
	uint32_t rasid;

	{
		char *ep;
		unsigned long v = strtoul(argv[1], &ep, 0);

		if (ep == argv[1]) {
			shell_print(sh, "error: invalid address '%s'", argv[1]);
			return -EINVAL;
		}
		vstart = (uintptr_t)v & ~(uintptr_t)(KB(4) - 1);
	}

	if (argc > 2) {
		char *ep;
		unsigned long v = strtoul(argv[2], &ep, 0);

		if (ep == argv[2]) {
			shell_print(sh, "error: invalid address '%s'", argv[2]);
			return -EINVAL;
		}
		vend = (uintptr_t)v & ~(uintptr_t)(KB(4) - 1);
	} else {
		vend = vstart;
	}

	if (vend < vstart)
		vend = vstart;

	rasid = shell_rsr_rasid();
	shell_print(sh, "RASID: 0x%08x", rasid);
	shell_print(sh, "  %-12s  %-12s  ring  asid  perms  cache");

	for (uintptr_t va = vstart; va <= vend; va += KB(4)) {
		uint32_t probe = shell_pdtlb((void *)va);

		if (!(probe & SHELL_PDTLB_HIT)) {
			shell_print(sh,
				    "  0x%08x   (DTLB miss — not in TLB cache)",
				    (uint32_t)va);
			continue;
		}

		uint32_t pte0 = shell_rdtlb0(probe);
		uint32_t pte1 = shell_rdtlb1(probe);
		uint32_t ring = (pte0 & SHELL_PTE_RING_MASK) >> SHELL_PTE_RING_SHIFT;
		uint32_t ca   = pte0 & SHELL_PTE_CA_MASK;
		uint32_t paddr = pte1 & SHELL_PTE_PPN_MASK;
		uint8_t  asid = (uint8_t)((rasid >> (ring * 8)) & 0xff);
		char perm[4] = {
			'R',
			(ca & XTENSA_MMU_PERM_W) ? 'W' : '-',
			(ca & XTENSA_MMU_PERM_X) ? 'X' : '-',
			'\0'
		};

		shell_print(sh,
			    "  0x%08x   0x%08x   %u     0x%02x  %s    %s",
			    (uint32_t)va, paddr,
			    ring, asid,
			    perm, ca_cache_str(ca));
	}
	return 0;
}

#endif /* CONFIG_XTENSA_MMU */

#endif /* CONFIG_SOF_SHELL_MMU_DBG */

#if CONFIG_SOF_SHELL_LLEXT_LOAD

#define SOF_SHELL_LLEXT_TIMEOUT_MSEC  120000U
#define SOF_SHELL_LLEXT_POLL_MSEC       500U

__cold static int cmd_sof_llext_load(const struct shell *sh,
				     size_t argc, char *argv[])
{
	const char *name = argv[1];
	uint32_t lib_id = 1;
	volatile struct sof_shell_llext_slot *slot;
	uint32_t elapsed = 0;
	uint32_t state;

	if (argc > 2) {
		long val;
		int ret = parse_long(sh, argv[2], &val, 1, LIB_MANAGER_MAX_LIBS - 1);

		if (ret)
			return ret;
		lib_id = (uint32_t)val;
	}

	/* Acquire or reuse the LLEXT_LOAD debug window slot */
#if CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
	{
		struct adsp_dw_desc slot_desc = { .type = ADSP_DW_SLOT_LLEXT_LOAD };
		size_t slot_size;

		slot = adsp_dw_request_slot(&slot_desc, &slot_size);
		if (!slot) {
			shell_error(sh, "Failed to acquire debug window slot");
			return -ENOMEM;
		}
	}
#else
	/* Fall back to a compile-time fixed slot index */
	slot = (volatile struct sof_shell_llext_slot *)
		ADSP_DW->slots[CONFIG_SOF_SHELL_LLEXT_LOAD_SLOT_NUM];
	ADSP_DW->descs[CONFIG_SOF_SHELL_LLEXT_LOAD_SLOT_NUM].type =
		ADSP_DW_SLOT_LLEXT_LOAD;
#endif

	state = slot->state;
	if (state != SOF_SHELL_LLEXT_IDLE) {
		shell_error(sh, "llext_load slot busy (state=%u) — try again later",
			    state);
		return -EBUSY;
	}

	/* Initialise the shared slot word-by-word (MMIO/uncached region) */
	{
		volatile uint32_t *p = (volatile uint32_t *)slot;
		size_t nwords = sizeof(struct sof_shell_llext_slot) / sizeof(uint32_t);

		for (size_t i = 0; i < nwords; i++)
			p[i] = 0;
	}
	strncpy((char *)slot->name, name, sizeof(slot->name) - 1);
	slot->lib_id = lib_id;
	slot->magic  = SOF_SHELL_LLEXT_MAGIC;
	/* Publish state last so the host only sees REQUESTING once all fields are set */
	slot->state  = SOF_SHELL_LLEXT_REQUESTING;

	shell_print(sh, "Slot ready: name=%s  lib_id=%u  timeout=%us",
		    name, lib_id, SOF_SHELL_LLEXT_TIMEOUT_MSEC / 1000);
	shell_print(sh, "On host:    dd if=<module.ri> of=/sys/kernel/debug/sof/llext_load bs=$(stat -c%%s <module.ri>) count=1");

	/* Poll waiting for the host to finish DMA + library load */
	while (elapsed < SOF_SHELL_LLEXT_TIMEOUT_MSEC) {
		k_msleep(SOF_SHELL_LLEXT_POLL_MSEC);
		elapsed += SOF_SHELL_LLEXT_POLL_MSEC;

		state = slot->state;

		if (state == SOF_SHELL_LLEXT_DMA_DONE) {
			shell_print(sh,
				    "llext_load OK: lib_id=%u  %u bytes transferred",
				    lib_id, slot->xfer_bytes);
			slot->state = SOF_SHELL_LLEXT_IDLE;
			return 0;
		}

		if (state == SOF_SHELL_LLEXT_ERROR) {
			shell_error(sh, "llext_load FAILED: result=%d", slot->result);
			slot->state = SOF_SHELL_LLEXT_IDLE;
			return (int)slot->result;
		}
	}

	shell_error(sh, "llext_load timeout after %us",
		    SOF_SHELL_LLEXT_TIMEOUT_MSEC / 1000);
	slot->state = SOF_SHELL_LLEXT_IDLE;
	return -ETIMEDOUT;
}

#endif /* CONFIG_SOF_SHELL_LLEXT_LOAD */

#if CONFIG_SOF_SHELL_LLEXT_LIST

/*
 * sof llext_list
 *
 * Lists all llext libraries currently held in IMR/DRAM.  For each library the
 * DRAM base address, total storage size and per-module-file name, text/data/BSS
 * sizes and SRAM state are printed.
 *
 * Example output:
 *   llext libs in IMR/DRAM:
 *   [1] base=0x89000000  size=49152 B  manifest_mods=1  elf_files=1
 *       [1:0] TESTER    text= 12288 B  data=  4096 B  bss=  4096 B  DRAM=yes  SRAM=no   use=0  dep=0
 */
__cold static int cmd_sof_llext_list(const struct shell *sh,
				     size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

#if CONFIG_LIBRARY_MANAGER
	struct ext_library *ext_lib = ext_lib_get();
	int found = 0;
	int lib_id;

	shell_print(sh, "llext libs in IMR/DRAM:");

	for (lib_id = 1; lib_id < LIB_MANAGER_MAX_LIBS; lib_id++) {
		const struct lib_manager_mod_ctx *ctx = ext_lib->desc[lib_id];
		const struct sof_man_fw_desc *desc;
		uint32_t store_bytes;

		if (!ctx || !ctx->base_addr)
			continue;

		desc = (const struct sof_man_fw_desc *)
			((const uint8_t *)ctx->base_addr + SOF_MAN_ELF_TEXT_OFFSET);
		store_bytes = desc->header.preload_page_count *
			      (uint32_t)_SHELL_MOD_PAGE_SZ;

		shell_print(sh, "[%d] base=%p  size=%u B  manifest_mods=%u  elf_files=%u",
			    lib_id, ctx->base_addr, store_bytes,
			    desc->header.num_module_entries,
			    ctx->n_mod);

#if CONFIG_LLEXT

		if (ctx->mod) {
			unsigned int i;

			for (i = 0; i < ctx->n_mod; i++) {
				const struct lib_manager_module *m = ctx->mod + i;
				const struct sof_man_module *mm;
				int use = m->llext ? (int)m->llext->use_count : 0;
				char name[SOF_MAN_MOD_NAME_LEN + 1];
				uint32_t text_sz = 0, data_sz = 0, bss_sz;
				int s;

				/* Always read the module manifest directly from the
				 * DRAM library descriptor. This is always valid after
				 * the library has been DMA'd to DRAM, and avoids cache
				 * coherency issues with the SRAM-linked copy. Use
				 * dcache_invalidate_region() to ensure we read the
				 * freshly-written DRAM content rather than a stale
				 * cache line.
				 * Note: m->mod_manifest (if non-NULL) points to the
				 * SRAM copy created during module instantiation; we
				 * still use text_size from there when available. */
				mm = (const struct sof_man_module *)
					((const uint8_t *)desc +
					 SOF_MAN_MODULE_OFFSET(i));
				dcache_invalidate_region(
					(__sparse_force void *)mm,
					sizeof(*mm));

				if (m->mod_manifest)
					text_sz = m->mod_manifest->text_size;

				memcpy(name, mm->name, SOF_MAN_MOD_NAME_LEN);
				name[SOF_MAN_MOD_NAME_LEN] = '\0';

				/* Scan manifest segments for page-based sizes.
				 * For DRAM-only llext modules these are 0; fall
				 * back to the runtime lib_manager segment sizes. */
				for (s = 0; s < 3; s++) {
					uint32_t pg = mm->segment[s].flags.r.length;

					switch (mm->segment[s].flags.r.type) {
					case SOF_MAN_SEGMENT_TEXT:
						if (!text_sz)
							text_sz = pg * _SHELL_MOD_PAGE_SZ;
						break;
					case SOF_MAN_SEGMENT_DATA: /* == RODATA */
						data_sz = pg * _SHELL_MOD_PAGE_SZ;
						break;
					default:
						break;
					}
				}
				bss_sz = (uint32_t)mm->instance_bss_size * _SHELL_MOD_PAGE_SZ;

				/* For DRAM llext modules the manifest page fields
				 * are 0; use the runtime lib_manager segment sizes
				 * if available. */
				if (!text_sz)
					text_sz = (uint32_t)m->segment[LIB_MANAGER_TEXT].size;
				if (!data_sz)
					data_sz = (uint32_t)(m->segment[LIB_MANAGER_DATA].size
							     + m->segment[LIB_MANAGER_RODATA].size);
				if (!bss_sz)
					bss_sz = (uint32_t)m->segment[LIB_MANAGER_BSS].size;

				shell_print(sh,
					    "    [%d:%u] %-8s"
					    "  text=%6u B  data=%6u B  bss=%6u B"
					    "  DRAM=yes  SRAM=%-3s"
					    "  use=%-2d  dep=%u",
					    lib_id, i, name,
					    text_sz, data_sz, bss_sz,
					    m->mapped ? "yes" : "no",
					    use,
					    m->n_dependent);
			}
		}
#endif /* CONFIG_LLEXT */

		found++;
	}

	if (!found)
		shell_print(sh, "  (none)");
#else
	shell_print(sh, "Library manager not enabled.");
#endif /* CONFIG_LIBRARY_MANAGER */
	return 0;
}

#endif /* CONFIG_SOF_SHELL_LLEXT_LIST */

#if CONFIG_SOF_SHELL_LLEXT_PURGE

/*
 * sof llext_purge <lib_id>
 *
 * Removes a loadable llext library from IMR/DRAM storage and frees its memory.
 * All module files belonging to the library must be unloaded from SRAM first
 * (i.e., all pipeline instances using the library must be torn down).
 *
 * Example:
 *   uart:~$ sof llext_purge 1
 *   llext_purge: lib 1 freed OK
 */
__cold static int cmd_sof_llext_purge(const struct shell *sh,
				      size_t argc, char *argv[])
{
#if CONFIG_LIBRARY_MANAGER
	char *endptr = NULL;
	long lib_id;
	int ret;

	lib_id = strtol(argv[1], &endptr, 0);
	if (endptr == argv[1] || lib_id < 1 || lib_id >= LIB_MANAGER_MAX_LIBS) {
		shell_error(sh, "lib_id must be 1..%d", LIB_MANAGER_MAX_LIBS - 1);
		return -EINVAL;
	}

	ret = lib_manager_purge_library((uint32_t)lib_id);
	switch (ret) {
	case 0:
		shell_print(sh, "llext_purge: lib %ld freed OK", lib_id);
		break;
	case -ENOENT:
		shell_error(sh, "llext_purge: lib %ld not loaded", lib_id);
		break;
	case -EBUSY:
		shell_error(sh, "llext_purge: lib %ld still active in SRAM — "
			    "destroy all pipelines using it first", lib_id);
		break;
	default:
		shell_error(sh, "llext_purge: lib %ld failed: %d", lib_id, ret);
		break;
	}
	return ret;
#else
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_print(sh, "Library manager not enabled.");
	return -ENOSYS;
#endif /* CONFIG_LIBRARY_MANAGER */
}

#endif /* CONFIG_SOF_SHELL_LLEXT_PURGE */

#if CONFIG_SOF_SHELL_LLEXT_SYMBOLS && CONFIG_LLEXT

/*
 * sof llext symbols [name]
 *
 * Without an argument: iterate every loaded Zephyr llext extension and for
 * each print its name, use_count, then two tables:
 *   exp_tab  – symbols exported by the extension via LL_EXTENSION_SYMBOL
 *   sym_tab  – all global symbols resolved during linking (internal + imports)
 *
 * With a name argument: restrict output to that single extension.
 *
 * Example (no arg):
 *   [tester]  use=1  exp=3  sym=12
 *     exp  my_api_init          0x60040100
 *     exp  my_api_process       0x60040120
 *     sym  __start              0x60040000
 *     ...
 */

struct _llext_sym_ctx {
	const struct shell *sh;
	const char *filter;  /* NULL = all */
	int found;
};

static int _llext_sym_cb(struct llext *ext, void *arg)
{
	struct _llext_sym_ctx *ctx = arg;
	size_t i;

	if (ctx->filter && strcmp(ext->name, ctx->filter) != 0)
		return 0;

	shell_print(ctx->sh,
		    "[%s]  use=%u  exp=%zu  sym=%zu",
		    ext->name, ext->use_count,
		    ext->exp_tab.sym_cnt, ext->sym_tab.sym_cnt);

	for (i = 0; i < ext->exp_tab.sym_cnt; i++)
		shell_print(ctx->sh, "  exp  %-32s  %p",
			    ext->exp_tab.syms[i].name,
			    ext->exp_tab.syms[i].addr);

	for (i = 0; i < ext->sym_tab.sym_cnt; i++)
		shell_print(ctx->sh, "  sym  %-32s  %p",
			    ext->sym_tab.syms[i].name,
			    ext->sym_tab.syms[i].addr);

	ctx->found++;
	return 0;
}

__cold static int cmd_sof_llext_symbols(const struct shell *sh,
					size_t argc, char *argv[])
{
	struct _llext_sym_ctx ctx = {
		.sh = sh,
		.filter = (argc > 1) ? argv[1] : NULL,
		.found = 0,
	};

	llext_iterate(_llext_sym_cb, &ctx);

	if (!ctx.found)
		shell_print(sh, argc > 1 ? "llext '%s' not found" : "(no llext loaded)",
			    argc > 1 ? argv[1] : "");
	return 0;
}

#endif /* CONFIG_SOF_SHELL_LLEXT_SYMBOLS && CONFIG_LLEXT */

static int cmd_sof_version(const struct shell *sh, size_t argc, char *argv[])
{
	shell_print(sh, "SOF Version: %d.%d.%d-%s (Build %d)",
		    SOF_MAJOR, SOF_MINOR, SOF_MICRO, SOF_TAG, SOF_BUILD);
	shell_print(sh, "Git:         %s", SOF_GIT_TAG);
	shell_print(sh, "Src hash:    0x%08x", SOF_SRC_HASH);
	shell_print(sh, "Zephyr:      %s (%s)", KERNEL_VERSION_STRING,
		    STRINGIFY(BUILD_VERSION));
	shell_print(sh, "Built:       " __DATE__ " " __TIME__);
	return 0;
}

static int cmd_sof_vpage_info(const struct shell *sh, size_t argc, char *argv[])
{
#if CONFIG_SOF_VREGIONS
	vpage_info(sh);
#else
	shell_fprintf(sh, SHELL_NORMAL, "Virtual regions not enabled\n");
#endif
	return 0;
}

static int cmd_sof_vregion_info(const struct shell *sh, size_t argc, char *argv[])
{
#if CONFIG_SOF_VREGIONS
	vregion_info_all(sh);
#else
	shell_fprintf(sh, SHELL_NORMAL, "Virtual regions not enabled\n");
#endif
	return 0;
}

#if CONFIG_SOF_SHELL_BUFFER_INFO

static void shell_print_buffer(const struct shell *sh, struct comp_buffer *buf,
			       uint32_t src_id, uint32_t sink_id)
{
	const struct audio_stream *s = &buf->stream;
	uint32_t size = audio_stream_get_size(s);
	uint32_t avail = audio_stream_get_avail(s);
	uint32_t freeb = audio_stream_get_free(s);

	shell_print(sh,
		    "  buf 0x%08x  src 0x%08x -> sink 0x%08x"
		    "  size %u  avail %u  free %u  ch %u  rate %u  fmt %d",
		    buf_get_id(buf), src_id, sink_id,
		    size, avail, freeb,
		    audio_stream_get_channels(s),
		    audio_stream_get_rate(s),
		    (int)audio_stream_get_frm_fmt(s));
}

/*
 * Walk every component in the IPC topology and visit each downstream
 * (bsink_list) buffer once. cb() is called for every (buffer, source, sink)
 * tuple. Returns the number of buffers visited.
 */
static int shell_for_each_buffer(struct ipc *ipc,
				 void (*cb)(const struct shell *sh,
					    struct comp_buffer *buf,
					    uint32_t src_id, uint32_t sink_id,
					    void *ctx),
				 const struct shell *sh, void *ctx)
{
	struct list_item *clist;
	struct ipc_comp_dev *icd;
	int count = 0;

	list_for_item(clist, &ipc->comp_list) {
		struct comp_dev *cd;
		struct comp_buffer *buf;

		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		cd = icd->cd;
		buf = comp_dev_get_first_data_consumer(cd);
		while (buf) {
			struct comp_dev *sink = comp_buffer_get_sink_component(buf);

			cb(sh, buf, cd->ipc_config.id,
			   sink ? sink->ipc_config.id : 0, ctx);
			count++;
			buf = comp_dev_get_next_data_consumer(cd, buf);
		}
	}

	return count;
}

static void buf_list_cb(const struct shell *sh, struct comp_buffer *buf,
			uint32_t src_id, uint32_t sink_id, void *ctx)
{
	ARG_UNUSED(ctx);
	shell_print_buffer(sh, buf, src_id, sink_id);
}

__cold static int cmd_sof_buffer_list(const struct shell *sh,
				      size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	int n;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	shell_print(sh, "Audio buffers:");
	n = shell_for_each_buffer(ipc, buf_list_cb, sh, NULL);
	if (!n)
		shell_print(sh, "  (none)");

	return 0;
}

struct buf_find_ctx {
	uint32_t want_id;
	struct comp_buffer *found;
	uint32_t src_id;
	uint32_t sink_id;
};

static void buf_find_cb(const struct shell *sh, struct comp_buffer *buf,
			uint32_t src_id, uint32_t sink_id, void *ctx)
{
	struct buf_find_ctx *c = ctx;

	ARG_UNUSED(sh);
	if (c->found)
		return;
	if (buf_get_id(buf) == c->want_id) {
		c->found = buf;
		c->src_id = src_id;
		c->sink_id = sink_id;
	}
}

__cold static int cmd_sof_buffer_info(const struct shell *sh,
				      size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	struct buf_find_ctx ctx = {0};
	const struct audio_stream *s;
	char *endptr = NULL;
	long id;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	id = strtol(argv[1], &endptr, 0);
	if (endptr == argv[1]) {
		shell_error(sh, "buffer_info: invalid id");
		return -EINVAL;
	}

	ctx.want_id = (uint32_t)id;
	shell_for_each_buffer(ipc, buf_find_cb, sh, &ctx);

	if (!ctx.found) {
		shell_print(sh, "buffer 0x%08x not found", (uint32_t)id);
		return -ENOENT;
	}

	s = &ctx.found->stream;
	shell_print(sh, "Buffer 0x%08x:", buf_get_id(ctx.found));
	shell_print(sh, "  source comp : 0x%08x", ctx.src_id);
	shell_print(sh, "  sink   comp : 0x%08x", ctx.sink_id);
	shell_print(sh, "  core        : %u", ctx.found->core);
	shell_print(sh, "  flags       : 0x%08x", ctx.found->flags);
	shell_print(sh, "  size bytes  : %u", audio_stream_get_size(s));
	shell_print(sh, "  avail bytes : %u", audio_stream_get_avail(s));
	shell_print(sh, "  free  bytes : %u", audio_stream_get_free(s));
	shell_print(sh, "  rptr        : %p", audio_stream_get_rptr(s));
	shell_print(sh, "  wptr        : %p", audio_stream_get_wptr(s));
	shell_print(sh, "  channels    : %u", audio_stream_get_channels(s));
	shell_print(sh, "  rate        : %u", audio_stream_get_rate(s));
	shell_print(sh, "  frame fmt   : %d", (int)audio_stream_get_frm_fmt(s));

	return 0;
}

#endif /* CONFIG_SOF_SHELL_BUFFER_INFO */

#if CONFIG_SOF_SHELL_SCHED_INFO

static const char *sched_type_str(int type)
{
	switch (type) {
	case SOF_SCHEDULE_EDF:		return "edf";
	case SOF_SCHEDULE_LL_TIMER:	return "ll_timer";
	case SOF_SCHEDULE_LL_DMA:	return "ll_dma";
	case SOF_SCHEDULE_DP:		return "dp";
	case SOF_SCHEDULE_TWB:		return "twb";
	default:			return "?";
	}
}

static const char *sched_state_str(enum task_state s)
{
	switch (s) {
	case SOF_TASK_STATE_INIT:	return "init";
	case SOF_TASK_STATE_QUEUED:	return "queued";
	case SOF_TASK_STATE_PENDING:	return "pending";
	case SOF_TASK_STATE_RUNNING:	return "running";
	case SOF_TASK_STATE_PREEMPTED:	return "preempt";
	case SOF_TASK_STATE_COMPLETED:	return "done";
	case SOF_TASK_STATE_FREE:	return "free";
	case SOF_TASK_STATE_CANCEL:	return "cancel";
	case SOF_TASK_STATE_RESCHEDULE:	return "resched";
	default:			return "?";
	}
}

struct sched_walk_ctx {
	const struct shell *sh;
	int sch_type;
	uint32_t total_sum;
	uint32_t total_cnt;
	uint32_t total_max;
	int task_count;
	bool show_load;
};

static void sched_list_cb(struct task *task, void *_ctx)
{
	struct sched_walk_ctx *c = _ctx;
	uint32_t avg = task->cycles_cnt ? task->cycles_sum / task->cycles_cnt : 0;

	if (c->show_load) {
		shell_print(c->sh,
			    "  %-9s core %u  prio %3u  state %-7s"
			    "  count %u  avg %u  max %u  sum %u cyc",
			    sched_type_str(c->sch_type), task->core,
			    task->priority, sched_state_str(task->state),
			    task->cycles_cnt, avg, task->cycles_max,
			    task->cycles_sum);
	} else {
		shell_print(c->sh,
			    "  %-9s core %u  prio %3u  state %-7s"
			    "  flags 0x%04x  uid %p  data %p",
			    sched_type_str(c->sch_type), task->core,
			    task->priority, sched_state_str(task->state),
			    task->flags, (const void *)task->uid, task->data);
	}

	c->total_sum += task->cycles_sum;
	c->total_cnt += task->cycles_cnt;
	if (task->cycles_max > c->total_max)
		c->total_max = task->cycles_max;
	c->task_count++;
}

static int sched_walk(const struct shell *sh, bool show_load)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct sched_walk_ctx ctx = { .sh = sh, .show_load = show_load };
	struct schedule_data *sch;
	struct list_item *slist;

	if (!schedulers) {
		shell_print(sh, "No schedulers registered");
		return 0;
	}

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (!sch->ops->scheduler_dump_tasks)
			continue;
		ctx.sch_type = sch->type;
		sch->ops->scheduler_dump_tasks(sch->data, sched_list_cb, &ctx);
	}

	if (!ctx.task_count)
		shell_print(sh, "  (no tasks)");

	if (show_load) {
		uint32_t avg = ctx.total_cnt ? ctx.total_sum / ctx.total_cnt : 0;

		shell_print(sh,
			    "Total: %d tasks  count %u  avg %u  peak max %u cyc",
			    ctx.task_count, ctx.total_cnt, avg, ctx.total_max);
	}

	return 0;
}

__cold static int cmd_sof_sched_tasks(const struct shell *sh,
				      size_t argc, char *argv[])
{
	shell_print(sh, "Active scheduler tasks:");
	return sched_walk(sh, false);
}

__cold static int cmd_sof_sched_load(const struct shell *sh,
				     size_t argc, char *argv[])
{
	shell_print(sh, "Scheduler task cycle counters:");
	return sched_walk(sh, true);
}

#endif /* CONFIG_SOF_SHELL_SCHED_INFO */

#if CONFIG_SOF_SHELL_LOG_INFO

#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_ctrl.h>

__cold static int cmd_sof_log_status(const struct shell *sh,
				     size_t argc, char *argv[])
{
	int n = log_backend_count_get();
	int i;

	shell_print(sh, "Log backends: %d, sources: %u",
		    n, log_src_cnt_get(Z_LOG_LOCAL_DOMAIN_ID));
	shell_print(sh, "  idx  id  active  name");

	for (i = 0; i < n; i++) {
		const struct log_backend *be = log_backend_get(i);

		if (!be)
			continue;
		shell_print(sh, "  %3d  %3u   %-3s    %s",
			    i, log_backend_id_get(be),
			    log_backend_is_active(be) ? "yes" : "no",
			    be->name ? be->name : "?");
	}

	return 0;
}

#endif /* CONFIG_SOF_SHELL_LOG_INFO */

#if CONFIG_SOF_SHELL_MTRACE_DUMP

#include <adsp_debug_window.h>

/* must match the layout used by zephyr/subsys/logging/backends/log_backend_adsp_mtrace.c */
struct sof_shell_mtrace_slot {
	uint32_t host_ptr;
	uint32_t dsp_ptr;
	uint8_t data[ADSP_DW_SLOT_SIZE - 2 * sizeof(uint32_t)];
} __packed;

#define SOF_SHELL_MTRACE_BUF_SIZE (ADSP_DW_SLOT_SIZE - 2 * sizeof(uint32_t))
#define SOF_SHELL_MTRACE_TYPE(core) \
	(ADSP_DW_SLOT_DEBUG_LOG | ((core) & ADSP_DW_SLOT_CORE_MASK))

__cold static int cmd_sof_mtrace_dump(const struct shell *sh,
				      size_t argc, char *argv[])
{
	struct sof_shell_mtrace_slot *slot;
	uint32_t r, w, len, i;

#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
	struct adsp_dw_desc desc = { .type = SOF_SHELL_MTRACE_TYPE(0) };

	slot = adsp_dw_request_slot(&desc, NULL);
#else
	slot = (struct sof_shell_mtrace_slot *)
		ADSP_DW->slots[ADSP_DW_SLOT_NUM_MTRACE];
#endif
	if (!slot) {
		shell_print(sh, "mtrace slot not available");
		return -ENODEV;
	}

	r = slot->host_ptr;
	w = slot->dsp_ptr;

	if (r == w) {
		shell_print(sh, "mtrace: empty (host_ptr=dsp_ptr=%u)", r);
		return 0;
	}

	if (w > r)
		len = w - r;
	else
		len = SOF_SHELL_MTRACE_BUF_SIZE - r + w;

	shell_print(sh,
		    "mtrace: host_ptr=%u dsp_ptr=%u unread=%u bytes (snapshot)",
		    r, w, len);

	/* print byte-by-byte without advancing host_ptr; preserves host consumer */
	for (i = 0; i < len; i++) {
		uint32_t off = (r + i) % SOF_SHELL_MTRACE_BUF_SIZE;

		shell_fprintf(sh, SHELL_NORMAL, "%c", slot->data[off]);
	}
	shell_fprintf(sh, SHELL_NORMAL, "\n");

	return 0;
}

#endif /* CONFIG_SOF_SHELL_MTRACE_DUMP */

#if CONFIG_SOF_SHELL_MAILBOX_HEX || CONFIG_SOF_SHELL_DBGWIN_DUMP

static void sof_shell_hex_dump(const struct shell *sh, uintptr_t base,
			       size_t off, size_t len)
{
	const uint8_t *p = (const uint8_t *)(base + off);
	size_t i, j;

	for (i = 0; i < len; i += 16) {
		size_t row = MIN((size_t)16, len - i);
		char ascii[17];

		shell_fprintf(sh, SHELL_NORMAL, "%08lx ",
			      (unsigned long)(off + i));
		for (j = 0; j < 16; j++) {
			if (j < row)
				shell_fprintf(sh, SHELL_NORMAL, " %02x", p[i + j]);
			else
				shell_fprintf(sh, SHELL_NORMAL, "   ");
			ascii[j] = (j < row && p[i + j] >= 0x20 && p[i + j] < 0x7f) ?
				   (char)p[i + j] : '.';
		}
		ascii[16] = '\0';
		shell_fprintf(sh, SHELL_NORMAL, "  %s\n", ascii);
	}
}

#endif

#if CONFIG_SOF_SHELL_MAILBOX_HEX

#include <sof/lib/mailbox.h>

struct sof_shell_mb_region {
	const char *name;
	uintptr_t base;
	size_t size;
};

static const struct sof_shell_mb_region sof_shell_mb_regions[] = {
#ifdef MAILBOX_EXCEPTION_BASE
	{ "exception", MAILBOX_EXCEPTION_BASE, MAILBOX_EXCEPTION_SIZE },
#endif
#ifdef MAILBOX_DSPBOX_BASE
	{ "dspbox",    MAILBOX_DSPBOX_BASE,    MAILBOX_DSPBOX_SIZE    },
#endif
#ifdef MAILBOX_HOSTBOX_BASE
	{ "hostbox",   MAILBOX_HOSTBOX_BASE,   MAILBOX_HOSTBOX_SIZE   },
#endif
#ifdef MAILBOX_DEBUG_BASE
	{ "debug",     MAILBOX_DEBUG_BASE,     MAILBOX_DEBUG_SIZE     },
#endif
};

__cold static int cmd_sof_mailbox_hex(const struct shell *sh,
				      size_t argc, char *argv[])
{
	const struct sof_shell_mb_region *r = NULL;
	size_t off = 0, len;
	char *end = NULL;
	int i;

	if (argc < 2) {
		shell_print(sh, "Mailbox regions:");
		for (i = 0; i < ARRAY_SIZE(sof_shell_mb_regions); i++)
			shell_print(sh, "  %-10s base 0x%08lx  size %zu",
				    sof_shell_mb_regions[i].name,
				    (unsigned long)sof_shell_mb_regions[i].base,
				    sof_shell_mb_regions[i].size);
		shell_print(sh, "Usage: sof mailbox_hex <region> [offset] [length]");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(sof_shell_mb_regions); i++) {
		if (!strcmp(argv[1], sof_shell_mb_regions[i].name)) {
			r = &sof_shell_mb_regions[i];
			break;
		}
	}
	if (!r) {
		shell_print(sh, "Unknown region '%s'", argv[1]);
		return -EINVAL;
	}

	if (argc > 2) {
		off = strtoul(argv[2], &end, 0);
		if (end == argv[2] || off >= r->size) {
			shell_print(sh, "Bad offset (max 0x%zx)", r->size);
			return -EINVAL;
		}
	}

	len = MIN((size_t)256, r->size - off);
	if (argc > 3) {
		len = strtoul(argv[3], &end, 0);
		if (end == argv[3])
			return -EINVAL;
		len = MIN(len, r->size - off);
	}

	shell_print(sh, "%s @ 0x%08lx + 0x%zx, %zu bytes:",
		    r->name, (unsigned long)r->base, off, len);
	sof_shell_hex_dump(sh, r->base, off, len);
	return 0;
}

#endif /* CONFIG_SOF_SHELL_MAILBOX_HEX */

#if CONFIG_SOF_SHELL_DBGWIN_DUMP

#include <adsp_memory.h>
#include <adsp_debug_window.h>

/* Mirror struct used by zephyr/soc/intel/intel_adsp/common/debug_window.c.
 * We map window 2 directly so we can read descriptors and slot data without
 * depending on slot-manager internals.
 */
struct sof_shell_dw {
	struct adsp_dw_desc descs[ADSP_DW_DESC_COUNT];
	uint8_t reserved[ADSP_DW_PAGE0_SLOT_OFFSET -
			 ADSP_DW_DESC_COUNT * sizeof(struct adsp_dw_desc)];
	uint8_t partial_page0[ADSP_DW_SLOT_SIZE - ADSP_DW_PAGE0_SLOT_OFFSET];
	uint8_t slots[ADSP_DW_SLOT_COUNT][ADSP_DW_SLOT_SIZE];
} __packed;

#define SOF_SHELL_DW_BASE \
	(DT_REG_ADDR(DT_PHANDLE(DT_NODELABEL(mem_window2), memory)) + WIN2_OFFSET)

static const char *dw_type_name(uint32_t type)
{
	switch (type & ADSP_DW_SLOT_TYPE_MASK) {
	case ADSP_DW_SLOT_UNUSED & ADSP_DW_SLOT_TYPE_MASK:
		return type ? "?" : "unused";
	case ADSP_DW_SLOT_CRITICAL_LOG & ADSP_DW_SLOT_TYPE_MASK:
		return "critical_log";
	case ADSP_DW_SLOT_DEBUG_LOG & ADSP_DW_SLOT_TYPE_MASK:
		return "debug_log";
	case ADSP_DW_SLOT_GDB_STUB & ADSP_DW_SLOT_TYPE_MASK:
		return "gdb_stub";
	case ADSP_DW_SLOT_TELEMETRY & ADSP_DW_SLOT_TYPE_MASK:
		return "telemetry";
	case ADSP_DW_SLOT_TRACE & ADSP_DW_SLOT_TYPE_MASK:
		return "trace";
	case ADSP_DW_SLOT_SHELL & ADSP_DW_SLOT_TYPE_MASK:
		return "shell";
	case ADSP_DW_SLOT_DEBUG_STREAM & ADSP_DW_SLOT_TYPE_MASK:
		return "debug_stream";
	case ADSP_DW_SLOT_BROKEN & ADSP_DW_SLOT_TYPE_MASK:
		return "broken";
	default:
		return "?";
	}
}

__cold static int cmd_sof_dbgwin_dump(const struct shell *sh,
				      size_t argc, char *argv[])
{
	volatile struct sof_shell_dw *dw =
		(volatile struct sof_shell_dw *)
		sys_cache_uncached_ptr_get((__sparse_force void __sparse_cache *)
					   SOF_SHELL_DW_BASE);
	int slot, i;
	size_t len = 256;
	char *end = NULL;

	if (argc < 2) {
		shell_print(sh,
			    "ADSP debug window @ 0x%08lx (%d slots, %u bytes each)",
			    (unsigned long)SOF_SHELL_DW_BASE,
			    ADSP_DW_SLOT_COUNT, ADSP_DW_SLOT_SIZE);
		shell_print(sh, "  slot  res_id      type       vma         name");
		for (i = 0; i < ADSP_DW_SLOT_COUNT; i++) {
			shell_print(sh,
				    "  %3d   0x%08x  0x%08x 0x%08x  %s (core %u)",
				    i, dw->descs[i].resource_id, dw->descs[i].type,
				    dw->descs[i].vma, dw_type_name(dw->descs[i].type),
				    (unsigned int)(dw->descs[i].type & ADSP_DW_SLOT_CORE_MASK));
		}
		shell_print(sh, "Usage: sof dbgwin_dump <slot> [length]");
		return 0;
	}

	slot = strtol(argv[1], &end, 0);
	if (end == argv[1] || slot < 0 || slot >= ADSP_DW_SLOT_COUNT) {
		shell_print(sh, "Bad slot (0..%d)", ADSP_DW_SLOT_COUNT - 1);
		return -EINVAL;
	}

	if (argc > 2) {
		len = strtoul(argv[2], &end, 0);
		if (end == argv[2])
			return -EINVAL;
	}
	len = MIN(len, (size_t)ADSP_DW_SLOT_SIZE);

	shell_print(sh, "Slot %d type=0x%08x (%s, core %u) vma=0x%08x; %zu bytes:",
		    slot, dw->descs[slot].type, dw_type_name(dw->descs[slot].type),
		    (unsigned int)(dw->descs[slot].type & ADSP_DW_SLOT_CORE_MASK),
		    dw->descs[slot].vma, len);
	sof_shell_hex_dump(sh, (uintptr_t)dw->slots[slot], 0, len);
	return 0;
}

#endif /* CONFIG_SOF_SHELL_DBGWIN_DUMP */

#if CONFIG_SOF_SHELL_PERF_STATUS

#include <sof/debug/telemetry/telemetry.h>
#include <sof/debug/telemetry/performance_monitor.h>
#include <ipc4/base_fw.h>

static const char *perf_state_str(enum ipc4_perf_measurements_state_set s)
{
	switch (s) {
	case IPC4_PERF_MEASUREMENTS_DISABLED:	return "disabled";
	case IPC4_PERF_MEASUREMENTS_STOPPED:	return "stopped";
	case IPC4_PERF_MEASUREMENTS_STARTED:	return "started";
	case IPC4_PERF_MEASUREMENTS_PAUSED:	return "paused";
	default:				return "?";
	}
}

__cold static int cmd_sof_perf_status(const struct shell *sh,
				      size_t argc, char *argv[])
{
	struct system_tick_info *systick;
	int core_id, ret;

	if (argc > 1) {
		if (!strcmp(argv[1], "reset")) {
			ret = reset_performance_counters();
			shell_print(sh, "perf: reset_performance_counters() = %d", ret);
			return ret;
		}
		if (!strcmp(argv[1], "start")) {
			ret = enable_performance_counters();
			if (!ret)
				perf_meas_set_state(IPC4_PERF_MEASUREMENTS_STARTED);
			shell_print(sh, "perf: enable_performance_counters() = %d", ret);
			return ret;
		}
		if (!strcmp(argv[1], "stop")) {
			perf_meas_set_state(IPC4_PERF_MEASUREMENTS_STOPPED);
			shell_print(sh, "perf: stopped");
			return 0;
		}
		if (!strcmp(argv[1], "pause")) {
			perf_meas_set_state(IPC4_PERF_MEASUREMENTS_PAUSED);
			shell_print(sh, "perf: paused");
			return 0;
		}
		shell_print(sh, "Usage: sof perf_status [reset|start|stop|pause]");
		return -EINVAL;
	}

	shell_print(sh, "Performance measurements: %s",
		    perf_state_str(perf_meas_get_state()));

#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
	systick = telemetry_get_systick_info_ptr();
	if (!systick) {
		shell_print(sh, "telemetry slot not allocated");
		return 0;
	}
#else
	{
		struct telemetry_wnd_data *wnd =
			(struct telemetry_wnd_data *)
			ADSP_DW->slots[SOF_DW_TELEMETRY_SLOT];
		systick = (struct system_tick_info *)wnd->system_tick_info;
	}
#endif

	shell_print(sh, "Per-core systick (count, last_us_cyc, max_us_cyc, avg_kcps, peak_kcps):");
	for (core_id = 0; core_id < CONFIG_MAX_CORE_COUNT; core_id++) {
		if (!(cpu_enabled_cores() & BIT(core_id)))
			continue;
		shell_print(sh,
			    "  core %u: count=%u last=%u max=%u avg_kcps=%u peak_kcps=%u peak4k=%u peak8k=%u",
			    core_id,
			    systick[core_id].count,
			    systick[core_id].last_time_elapsed,
			    systick[core_id].max_time_elapsed,
			    systick[core_id].avg_utilization,
			    systick[core_id].peak_utilization,
			    systick[core_id].peak_utilization_4k,
			    systick[core_id].peak_utilization_8k);
	}

	return 0;
}

#endif /* CONFIG_SOF_SHELL_PERF_STATUS */

#if CONFIG_SOF_SHELL_DAI_LIST || CONFIG_SOF_SHELL_DMA_STATUS
#include <sof/lib/dai.h>
#include <zephyr/drivers/dai.h>
#include <zephyr/drivers/dma.h>
#endif

#if CONFIG_SOF_SHELL_DAI_LIST

static const char *zephyr_dai_type_str(int t)
{
	switch (t) {
	case DAI_LEGACY_I2S:	return "i2s";
	case DAI_INTEL_SSP:	return "ssp";
	case DAI_INTEL_DMIC:	return "dmic";
	case DAI_INTEL_HDA:	return "hda";
	case DAI_INTEL_ALH:	return "alh";
	case DAI_IMX_SAI:	return "sai";
	case DAI_IMX_ESAI:	return "esai";
	case DAI_AMD_BT:	return "amd_bt";
	case DAI_AMD_SP:	return "amd_sp";
	case DAI_AMD_DMIC:	return "amd_dmic";
	case DAI_MEDIATEK_AFE:	return "mtk_afe";
	case DAI_INTEL_SSP_NHLT:  return "ssp_nhlt";
	case DAI_INTEL_DMIC_NHLT: return "dmic_nhlt";
	case DAI_INTEL_HDA_NHLT:  return "hda_nhlt";
	case DAI_INTEL_ALH_NHLT:  return "alh_nhlt";
	case DAI_IMX_MICFIL:	return "micfil";
	case DAI_INTEL_UAOL:	return "uaol";
	case DAI_AMD_SDW:	return "amd_sdw";
	default:		return "?";
	}
}

__cold static int cmd_sof_dai_list_fn(const struct shell *sh,
				      size_t argc, char *argv[])
{
	const struct device **list;
	size_t count = 0;
	int i;

	list = dai_get_device_list(&count);
	if (!list || !count) {
		shell_print(sh, "No DAIs registered");
		return 0;
	}

	shell_print(sh, "%zu DAI(s) registered:", count);
	shell_print(sh, "  idx  name                       type        index  channels  rate     fmt    word");
	for (i = 0; i < count; i++) {
		const struct device *dev = list[i];
		struct dai_config cfg = {0};
		const struct dai_properties *props;

		if (dai_config_get(dev, &cfg, DAI_DIR_BOTH)) {
			/* try TX-only then RX-only */
			if (dai_config_get(dev, &cfg, DAI_DIR_TX) &&
			    dai_config_get(dev, &cfg, DAI_DIR_RX)) {
				shell_print(sh, "  %3d  %-26s  (config_get failed)",
					    i, dev->name ? dev->name : "?");
				continue;
			}
		}

		shell_print(sh,
			    "  %3d  %-26s  %-10s  %5u  %8u  %7u  0x%04x %4u",
			    i, dev->name ? dev->name : "?",
			    zephyr_dai_type_str(cfg.type), cfg.dai_index,
			    cfg.channels, cfg.rate, cfg.format, cfg.word_size);

		/* get_properties is optional and only valid on configured DAIs */
		if (cfg.channels && DEVICE_API_GET(dai, dev)->get_properties) {
			props = dai_get_properties(dev, DAI_DIR_TX, 0);
			if (props)
				shell_print(sh,
					    "        TX: fifo 0x%08x depth %u hs %u stream %d",
					    props->fifo_address, props->fifo_depth,
					    props->dma_hs_id, props->stream_id);
			props = dai_get_properties(dev, DAI_DIR_RX, 0);
			if (props)
				shell_print(sh,
					    "        RX: fifo 0x%08x depth %u hs %u stream %d",
					    props->fifo_address, props->fifo_depth,
					    props->dma_hs_id, props->stream_id);
		}
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sof_dai_commands,
	SHELL_CMD(list, NULL,
		  "List all registered DAI endpoints: name, type, index, "
		  "channels, rate, format, word size and per-direction fifo/hs\n",
		  cmd_sof_dai_list_fn),
	SHELL_SUBCMD_SET_END
);

#endif /* CONFIG_SOF_SHELL_DAI_LIST */

#if CONFIG_SOF_SHELL_DMA_STATUS

static const char *dma_dir_str(enum dma_channel_direction d)
{
	switch (d) {
	case MEMORY_TO_MEMORY:	return "M2M";
	case MEMORY_TO_PERIPHERAL: return "M2P";
	case PERIPHERAL_TO_MEMORY: return "P2M";
	case PERIPHERAL_TO_PERIPHERAL: return "P2P";
	case HOST_TO_MEMORY:	return "H2M";
	case MEMORY_TO_HOST:	return "M2H";
	default:		return "?";
	}
}

static void dma_print_one(const struct shell *sh, struct sof_dma *dma,
			  int dma_idx, int chan)
{
	struct dma_status st = {0};
	int ret = sof_dma_get_status(dma, chan, &st);

	if (ret) {
		shell_print(sh, "  dma %d ch %d: get_status -> %d",
			    dma_idx, chan, ret);
		return;
	}
	shell_print(sh,
		    "  dma %d ch %d: %s dir=%s pending=%u free=%u rd=%u wr=%u total=%llu",
		    dma_idx, chan, st.busy ? "BUSY" : "idle",
		    dma_dir_str(st.dir), st.pending_length, st.free,
		    st.read_position, st.write_position,
		    (unsigned long long)st.total_copied);
}

__cold static int cmd_sof_dma_status(const struct shell *sh,
				     size_t argc, char *argv[])
{
	const struct dma_info *info = dma_info_get();
	struct sof_dma *dma;
	int i, ch;

	if (!info || !info->num_dmas) {
		shell_print(sh, "No DMA controllers registered");
		return 0;
	}

	if (argc == 1) {
		shell_print(sh, "%zu DMA controller(s):", info->num_dmas);
		shell_print(sh, "  idx  id  channels  busy  caps     devs     base");
		for (i = 0; i < info->num_dmas; i++) {
			dma = &info->dma_array[i];
			shell_print(sh,
				    "  %3d  %2u  %8u  %4u  0x%04x   0x%04x   0x%08x  (%s)",
				    i, dma->plat_data.id,
				    dma->plat_data.channels,
				    (unsigned int)atomic_get(&dma->num_channels_busy),
				    dma->plat_data.caps,
				    dma->plat_data.devs,
				    dma->plat_data.base,
				    dma->z_dev && dma->z_dev->name ?
					dma->z_dev->name : "?");
		}
		shell_print(sh,
			    "Usage: sof dma_status <dma_idx> [chan]  (omit chan to walk all)");
		return 0;
	}

	{
		char *end = NULL;
		long idx = strtol(argv[1], &end, 0);

		if (end == argv[1] || idx < 0 || idx >= (long)info->num_dmas) {
			shell_print(sh, "Bad DMA index (0..%zu)",
				    info->num_dmas - 1);
			return -EINVAL;
		}
		dma = &info->dma_array[idx];

		if (argc > 2) {
			ch = strtol(argv[2], &end, 0);
			if (end == argv[2] || ch < 0 ||
			    ch >= (int)dma->plat_data.channels) {
				shell_print(sh, "Bad channel (0..%u)",
					    dma->plat_data.channels - 1);
				return -EINVAL;
			}
			dma_print_one(sh, dma, (int)idx, ch);
			return 0;
		}

		shell_print(sh, "DMA %ld (%s): %u channels",
			    idx, dma->z_dev && dma->z_dev->name ?
				 dma->z_dev->name : "?",
			    dma->plat_data.channels);
		for (ch = 0; ch < (int)dma->plat_data.channels; ch++)
			dma_print_one(sh, dma, (int)idx, ch);
	}

	return 0;
}

#endif /* CONFIG_SOF_SHELL_DMA_STATUS */

#if CONFIG_SOF_SHELL_KCTL_LIST

/*
 * Best-effort decoded driver name. Module-adapter components share
 * SOF_COMP_MODULE_ADAPTER for drv->type, so the only stable per-module
 * label available in firmware is the UUID name string from the trace
 * context (which carries the same name printed by the LDC tool).
 */
static const char *kctl_drv_name(const struct comp_dev *cd)
{
	if (cd && cd->drv && cd->drv->tctx && cd->drv->tctx->uuid_p &&
	    cd->drv->tctx->uuid_p->name[0])
		return cd->drv->tctx->uuid_p->name;
	return "?";
}

/*
 * Tag the modules that are known to expose ALSA-style kcontrols
 * (volume / gain / mixer-style switches and enums). This is purely a
 * UI hint -- the actual control values live behind per-module
 * config_id blobs that need IPC4 large_config marshalling, which is
 * intentionally out of scope here (see shell.md).
 */
static const char *kctl_drv_kind(const char *name)
{
	if (!name)
		return "";
	if (!strcmp(name, "volume") || !strcmp(name, "gain"))
		return "volume";
	if (!strcmp(name, "mixin") || !strcmp(name, "mixout") ||
	    !strcmp(name, "mixer"))
		return "mixer";
	if (!strcmp(name, "eqiir") || !strcmp(name, "eqfir") ||
	    !strcmp(name, "drc") || !strcmp(name, "multiband_drc") ||
	    !strcmp(name, "dcblock") || !strcmp(name, "tdfb") ||
	    !strcmp(name, "crossover") || !strcmp(name, "google_rtc_audio_processing"))
		return "blob";
	if (!strcmp(name, "selector") || !strcmp(name, "src") ||
	    !strcmp(name, "asrc"))
		return "config";
	return "";
}

__cold static int cmd_sof_kctl_list(const struct shell *sh,
				    size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	struct list_item *clist;
	struct ipc_comp_dev *icd;
	int count = 0;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	shell_print(sh, "%-12s %-8s %-5s %-24s %-8s %s",
		    "comp_id", "ppl_id", "core", "module", "kind", "state");

	list_for_item(clist, &ipc->comp_list) {
		const struct comp_dev *cd;
		const char *name;

		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		cd = icd->cd;
		name = kctl_drv_name(cd);

		shell_print(sh, "0x%-10x %-8u %-5u %-24s %-8s %s",
			    icd->id,
			    cd->pipeline ? cd->pipeline->pipeline_id : 0,
			    icd->core, name, kctl_drv_kind(name),
			    comp_state_str(cd->state));
		count++;
	}

	if (!count) {
		shell_print(sh,
			    "No components found. Start an audio stream first.");
		return 0;
	}

	shell_print(sh, "");
	shell_print(sh,
		    "kctl get/set is intentionally not exposed here -- control");
	shell_print(sh,
		    "values flow through per-module IPC4 large_config blobs");
	shell_print(sh,
		    "(set_configuration / get_configuration). Use tinymix /");
	shell_print(sh,
		    "sof-ctl on the host, or 'sof module_status' for raw state.");

	return 0;
}

#endif /* CONFIG_SOF_SHELL_KCTL_LIST */

SHELL_STATIC_SUBCMD_SET_CREATE(sof_vpage_commands,
	SHELL_CMD(info, NULL,
		  "Print virtual page allocator status\n",
		  cmd_sof_vpage_info),
	SHELL_SUBCMD_SET_END
);

static int cmd_sof_ipc_stats(const struct shell *sh, size_t argc, char *argv[])
{
	struct ipc_stats s;

	if (argc > 1 && !strcmp(argv[1], "reset")) {
		ipc_stats_reset();
		shell_print(sh, "ipc stats reset");
		return 0;
	}

	ipc_stats_get(&s);
	shell_print(sh, "IPC statistics:");
	shell_print(sh, "  rx_count        : %u", s.rx_count);
	shell_print(sh, "  rx_errors       : %u", s.rx_errors);
	shell_print(sh, "  tx_count        : %u", s.tx_count);
	shell_print(sh, "  tx_direct_count : %u", s.tx_direct_count);
	shell_print(sh, "  tx_errors       : %u", s.tx_errors);
	return 0;
}

static int cmd_sof_ipc_last(const struct shell *sh, size_t argc, char *argv[])
{
	struct ipc_stats s;

	ipc_stats_get(&s);
	shell_print(sh, "Last IPC RX: pri=0x%08x ext=0x%08x @ %llu cycles",
		    s.last_rx_pri, s.last_rx_ext, (unsigned long long)s.last_rx_time);
	shell_print(sh, "Last IPC TX: pri=0x%08x ext=0x%08x @ %llu cycles",
		    s.last_tx_pri, s.last_tx_ext, (unsigned long long)s.last_tx_time);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sof_vreg_commands,
	SHELL_CMD(info, NULL,
		  "Print virtual regions status\n",
		  cmd_sof_vregion_info),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_llext_commands,
#if CONFIG_SOF_SHELL_LLEXT_LOAD
	SHELL_CMD_ARG(load, NULL,
		  "Load llext module from host: <name> [lib_id=1]\n"
		  "Sets up the DMA handshake slot then waits for:\n"
		  "  dd if=<module.ri> of=/sys/kernel/debug/sof/llext_load\\\n"
		  "     bs=$(stat -c%s <module.ri>) count=1\n"
		  "on the host. Prints result when DMA and IPC4 load complete.\n",
		  cmd_sof_llext_load, 2, 1),
#endif
#if (CONFIG_SOF_SHELL_LLEXT_LIST || CONFIG_SOF_SHELL_LLEXT_PURGE) && CONFIG_LLEXT
	SHELL_CMD(list, NULL,
		  "List llext libraries stored in IMR/DRAM.\n"
		  "For each library shows base address, storage size and per-module\n"
		  "SRAM mapping state (yes/no), use count and dependency count.\n",
		  cmd_sof_llext_list),
#endif
#if CONFIG_SOF_SHELL_LLEXT_PURGE
	SHELL_CMD_ARG(purge, NULL,
		  "Purge llext library from IMR/DRAM: <lib_id>\n"
		  "Fails with -EBUSY if any module in the library is still\n"
		  "mapped in SRAM (i.e. a pipeline using it is still active).\n",
		  cmd_sof_llext_purge, 2, 0),
#endif
#if CONFIG_SOF_SHELL_LLEXT_SYMBOLS && CONFIG_LLEXT
	SHELL_CMD_ARG(symbols, NULL,
		  "List symbol tables of all (or one named) llext: [name]\n"
		  "Shows exp_tab (exported) and sym_tab (all linked) symbols\n"
		  "with address, plus extension use_count.\n",
		  cmd_sof_llext_symbols, 1, 1),
#endif
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_test_ll_commands,
	SHELL_CMD(delay, NULL,
		  "Inject a scheduling gap to stress the LL timer domain\n",
		  cmd_sof_test_inject_sched_gap),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_test_commands,
	SHELL_CMD(ll, &sof_test_ll_commands,
		  "LL timer domain test commands\n",
		  NULL),
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
#if CONFIG_SOF_SHELL_CORE_POWER
	SHELL_CMD_ARG(on, NULL,
		  "Power on a secondary DSP core: <core_id>\n"
		  "core_id must be 1..CONFIG_CORE_COUNT-1 (core 0 is primary).\n",
		  cmd_sof_core_on, 2, 0),
	SHELL_CMD_ARG(off, NULL,
		  "Power off a secondary DSP core: <core_id>\n"
		  "core_id must be 1..CONFIG_CORE_COUNT-1 (core 0 is primary).\n",
		  cmd_sof_core_off, 2, 0),
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

#if CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB
SHELL_STATIC_SUBCMD_SET_CREATE(sof_mmu_commands,
	SHELL_CMD(info, NULL,
		  "Print Intel ADSP MTL TLB / virtual memory status\n",
		  cmd_sof_mmu_status),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_tlb_commands,
	SHELL_CMD(info, NULL,
		  "Dump all active TLB entries (vaddr/paddr/flags)\n",
		  cmd_sof_tlb_dump),
	SHELL_CMD_ARG(lookup, NULL,
		  "Query TLB for a page or range: <vaddr> [end_vaddr]\n",
		  cmd_sof_tlb_lookup, 2, 1),
	SHELL_SUBCMD_SET_END
);
#endif /* CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB */

#if CONFIG_SOF_SHELL_MMU_DBG && CONFIG_XTENSA_MMU
SHELL_STATIC_SUBCMD_SET_CREATE(sof_page_commands,
	SHELL_CMD_ARG(info, NULL,
		  "Probe DTLB: PA/ring/ASID/perms/cache per page (XTENSA_MMU): "
		  "<vaddr> [end_vaddr]\n",
		  cmd_sof_page_info, 2, 1),
	SHELL_SUBCMD_SET_END
);
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(sof_ipc_commands,
	SHELL_CMD_ARG(stats, NULL,
		  "Print IPC RX/TX counters; 'sof ipc stats reset' clears them\n",
		  cmd_sof_ipc_stats, 1, 1),
	SHELL_CMD(last, NULL,
		  "Print the last received and sent IPC headers\n",
		  cmd_sof_ipc_last),
	SHELL_SUBCMD_SET_END
);

#if CONFIG_SOF_SHELL_BUFFER_INFO
SHELL_STATIC_SUBCMD_SET_CREATE(sof_buffer_commands,
	SHELL_CMD(list, NULL,
		  "List all audio buffers (id, source/sink, fill, format)\n",
		  cmd_sof_buffer_list),
	SHELL_CMD_ARG(info, NULL,
		  "Detailed info for a single buffer: <buffer_id>\n",
		  cmd_sof_buffer_info, 2, 0),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_SCHED_INFO
SHELL_STATIC_SUBCMD_SET_CREATE(sof_sched_commands,
	SHELL_CMD(tasks, NULL,
		  "List all scheduler tasks (type, core, prio, state)\n",
		  cmd_sof_sched_tasks),
	SHELL_CMD(load, NULL,
		  "Show per-task cycle counters and totals\n",
		  cmd_sof_sched_load),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_LOG_INFO
SHELL_STATIC_SUBCMD_SET_CREATE(sof_log_commands,
	SHELL_CMD(status, NULL,
		  "List Zephyr log backends with state and source count\n",
		  cmd_sof_log_status),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_MTRACE_DUMP
SHELL_STATIC_SUBCMD_SET_CREATE(sof_mtrace_commands,
	SHELL_CMD(dump, NULL,
		  "Snapshot the mtrace SRAM ring buffer (does not advance host_ptr)\n",
		  cmd_sof_mtrace_dump),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_MAILBOX_HEX
SHELL_STATIC_SUBCMD_SET_CREATE(sof_mailbox_commands,
	SHELL_CMD_ARG(hex, NULL,
		  "Hex-dump a mailbox region: <region> [offset] [length]\n",
		  cmd_sof_mailbox_hex, 1, 3),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_DBGWIN_DUMP
SHELL_STATIC_SUBCMD_SET_CREATE(sof_dbgwin_commands,
	SHELL_CMD_ARG(dump, NULL,
		  "List ADSP debug-window slots, or hex-dump one: [slot] [length]\n",
		  cmd_sof_dbgwin_dump, 1, 2),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_PERF_STATUS
SHELL_STATIC_SUBCMD_SET_CREATE(sof_perf_commands,
	SHELL_CMD_ARG(status, NULL,
		  "Show telemetry perf state and per-core systick;"
		  " optional [reset|start|stop|pause]\n",
		  cmd_sof_perf_status, 1, 1),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_DMA_STATUS
SHELL_STATIC_SUBCMD_SET_CREATE(sof_dma_commands,
	SHELL_CMD_ARG(status, NULL,
		  "List DMA controllers, or per-channel status: "
		  "[dma_idx] [chan]\n",
		  cmd_sof_dma_status, 1, 2),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_KCTL_LIST
SHELL_STATIC_SUBCMD_SET_CREATE(sof_kctl_commands,
	SHELL_CMD(list, NULL,
		  "List components and their decoded module name / kind\n",
		  cmd_sof_kctl_list),
	SHELL_SUBCMD_SET_END
);
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(sof_commands,
	SHELL_CMD(test, &sof_test_commands,
		  "Test commands: ll delay\n",
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

#if CONFIG_SOF_SHELL_CORE_STATUS || CONFIG_SOF_SHELL_CORE_POWER
	SHELL_CMD(core, &sof_core_commands,
		  "Core commands: info, on, off\n",
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

#if CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB
	SHELL_CMD(mmu, &sof_mmu_commands,
		  "MMU commands: info (CONFIG_SOF_SHELL_MMU_DBG)\n",
		  NULL),
	SHELL_CMD(tlb, &sof_tlb_commands,
		  "TLB commands: info, lookup (CONFIG_SOF_SHELL_MMU_DBG)\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_MMU_DBG && CONFIG_XTENSA_MMU
	SHELL_CMD(rasid, NULL,
		  "Decode RASID register: ring 0-3 to ASID mapping (XTENSA_MMU)\n",
		  cmd_sof_rasid),
	SHELL_CMD(page, &sof_page_commands,
		  "Page commands: info (XTENSA_MMU)\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_LLEXT_LOAD || \
    ((CONFIG_SOF_SHELL_LLEXT_LIST || CONFIG_SOF_SHELL_LLEXT_PURGE || \
      CONFIG_SOF_SHELL_LLEXT_SYMBOLS) && CONFIG_LLEXT)
	SHELL_CMD(llext, &sof_llext_commands,
		  "Llext commands: load, list, purge, symbols (SOF_SHELL_LLEXT_*)\n",
		  NULL),
#endif

	SHELL_CMD(version, NULL,
		  "Print the current SOF software version\n",
		  cmd_sof_version),

	SHELL_CMD(vpage, &sof_vpage_commands,
		  "Vpage commands: info (CONFIG_SOF_SHELL_VPAGE_INFO, SOF_VREGIONS)\n",
		  NULL),

	SHELL_CMD(vreg, &sof_vreg_commands,
		  "Vregion commands: info (CONFIG_SOF_SHELL_VREGION_INFO, SOF_VREGIONS)\n",
		  NULL),

	SHELL_CMD(ipc, &sof_ipc_commands,
		  "IPC commands: stats, last\n",
		  NULL),

#if CONFIG_SOF_SHELL_BUFFER_INFO
	SHELL_CMD(buffer, &sof_buffer_commands,
		  "Buffer commands: list, info\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_SCHED_INFO
	SHELL_CMD(sched, &sof_sched_commands,
		  "Scheduler commands: tasks, load\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_LOG_INFO
	SHELL_CMD(log, &sof_log_commands,
		  "Log commands: status\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_MTRACE_DUMP
	SHELL_CMD(mtrace, &sof_mtrace_commands,
		  "Mtrace commands: dump\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_MAILBOX_HEX
	SHELL_CMD(mailbox, &sof_mailbox_commands,
		  "Mailbox commands: hex\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_DBGWIN_DUMP
	SHELL_CMD(dbgwin, &sof_dbgwin_commands,
		  "Debug window commands: dump\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_PERF_STATUS
	SHELL_CMD(perf, &sof_perf_commands,
		  "Perf commands: status\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_DAI_LIST
	SHELL_CMD(dai, &sof_dai_commands,
		  "DAI commands: list\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_DMA_STATUS
	SHELL_CMD(dma, &sof_dma_commands,
		  "DMA commands: status\n",
		  NULL),
#endif

#if CONFIG_SOF_SHELL_KCTL_LIST
	SHELL_CMD(kctl, &sof_kctl_commands,
		  "Kcontrol commands: list\n",
		  NULL),
#endif

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sof, &sof_commands,
		   "SOF application commands", NULL);

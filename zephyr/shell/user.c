// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
 */

/*
 * SOF shell - audio domain commands.
 *
 * This file hosts the user/audio facing shell commands (pipelines, modules,
 * buffers, DAI/DMA, kcontrols). They attach to the root "sof" command that is
 * created in shell/kernel.c via the shell iterable-section subcommand
 * mechanism.
 */

#include <rtos/sof.h> /* sof_get() */
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/topology.h>
#include <sof/lib/memory.h>
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

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>

#include <stdlib.h>

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

SHELL_SUBCMD_ADD((sof), module_heap_usage, NULL,
		 "Print heap memory usage of each module\n",
		 cmd_sof_module_heap_usage, 0, 0);
#endif /* CONFIG_SOF_SHELL_HEAP_USAGE */

#if CONFIG_SOF_SHELL_PIPELINE_STATUS || CONFIG_SOF_SHELL_MODULE_STATUS

__cold_rodata static const char * const comp_state_names[] = {
	[COMP_STATE_NOT_EXIST]	= "not_exist",
	[COMP_STATE_INIT]	= "init",
	[COMP_STATE_READY]	= "ready",
	[COMP_STATE_SUSPEND]	= "suspend",
	[COMP_STATE_PREPARE]	= "prepare",
	[COMP_STATE_PAUSED]	= "paused",
	[COMP_STATE_ACTIVE]	= "active",
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

SHELL_SUBCMD_ADD((sof), pipeline_status, NULL,
		 "Print status of all active pipelines\n",
		 cmd_sof_pipeline_status, 0, 0);
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

SHELL_SUBCMD_ADD((sof), module_status, NULL,
		 "Print status of all active components\n",
		 cmd_sof_module_status, 0, 0);
#endif /* CONFIG_SOF_SHELL_MODULE_STATUS */

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
					  int lib_id, bool verbose)
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

		if (!verbose) {
			shell_print(sh,
				    "[%d:%d] %-8s  inst:%-3u  cpc:%-8u  text:%-7u  bss:%u",
				    lib_id, i, name,
				    mod->instance_max_count,
				    cfg ? cfg->cpc : 0U,
				    text_sz, bss_sz);
			continue;
		}

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
	bool verbose = (argc >= 2 && strcmp(argv[1], "-v") == 0);
	int total = 0;

	shell_print(sh, "Built-in modules:");
	desc = basefw_vendor_get_manifest();
	if (desc) {
		print_manifest_modules(sh, desc, 0, verbose);
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
			print_manifest_modules(sh, desc, lib_id, verbose);
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

SHELL_SUBCMD_ADD((sof), module_list, NULL,
		 "List all available modules (name, inst, cpc, text, bss)\n"
		 "  [-v]  also show uuid, affinity, cps, ibs, obs\n",
		 cmd_sof_module_list, 1, 1);
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

#if CONFIG_SOF_SHELL_PIPELINE_OPS
SHELL_SUBCMD_ADD((sof), ppl_create, NULL,
		 "Create IPC4 pipeline: <ppl_id> [priority=0] [pages=2] [core=0] [lp=0]\n",
		 cmd_sof_ppl_create, 2, 4);
SHELL_SUBCMD_ADD((sof), ppl_delete, NULL,
		 "Delete IPC4 pipeline: <ppl_id>\n",
		 cmd_sof_ppl_delete, 2, 0);
SHELL_SUBCMD_ADD((sof), ppl_state, NULL,
		 "Set IPC4 pipeline state: <ppl_id> <running|paused|reset>\n",
		 cmd_sof_ppl_state, 3, 0);
SHELL_SUBCMD_ADD((sof), mod_init, NULL,
		 "Instantiate module: <mod_id> <inst_id> <ppl_id> [core=0] [dp=0]\n",
		 cmd_sof_mod_init, 4, 2);
SHELL_SUBCMD_ADD((sof), mod_delete, NULL,
		 "Delete module instance: <mod_id> <inst_id>\n",
		 cmd_sof_mod_delete, 3, 0);
SHELL_SUBCMD_ADD((sof), mod_bind, NULL,
		 "Bind two module instances: <src_mod> <src_inst> <dst_mod> <dst_inst> [src_q=0] [dst_q=0]\n",
		 cmd_sof_mod_bind, 5, 2);
SHELL_SUBCMD_ADD((sof), mod_unbind, NULL,
		 "Unbind two module instances: <src_mod> <src_inst> <dst_mod> <dst_inst> [src_q=0] [dst_q=0]\n",
		 cmd_sof_mod_unbind, 5, 2);
#endif /* CONFIG_SOF_SHELL_PIPELINE_OPS */

__cold static int cmd_sof_pipeline_list(const struct shell *sh, size_t argc, char *argv[])
{
	struct ipc *ipc = sof_get()->ipc;
	struct list_item *clist;
	struct ipc_comp_dev *icd;
	struct pipeline *p;

	if (!ipc) {
		shell_print(sh, "No IPC");
		return 0;
	}

	shell_print(sh, "ID          Core  Status  Priority  Period");
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_PIPELINE)
			continue;

		p = icd->pipeline;
		shell_print(sh, "0x%08x  %d     %d       %d         %d",
			    p->pipeline_id, p->core, p->status, p->priority, p->period);
	}
	return 0;
}

SHELL_SUBCMD_ADD((sof), pipeline_list, NULL,
		 "List all active audio pipelines\n",
		 cmd_sof_pipeline_list, 0, 0);

#if CONFIG_SOF_SHELL_BUFFER_INFO

__cold static void shell_print_buffer(const struct shell *sh, struct comp_buffer *buf,
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
__cold static int shell_for_each_buffer(struct ipc *ipc,
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

__cold static void buf_list_cb(const struct shell *sh, struct comp_buffer *buf,
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

__cold static void buf_find_cb(const struct shell *sh, struct comp_buffer *buf,
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

#if CONFIG_SOF_SHELL_BUFFER_INFO
SHELL_SUBCMD_ADD((sof), buffer_list, NULL,
		 "List all audio buffers (id, source/sink, fill, format)\n",
		 cmd_sof_buffer_list, 0, 0);
SHELL_SUBCMD_ADD((sof), buffer_info, NULL,
		 "Detailed info for a single buffer: <buffer_id>\n",
		 cmd_sof_buffer_info, 2, 0);
#endif

#if CONFIG_SOF_SHELL_DAI_LIST || CONFIG_SOF_SHELL_DMA_STATUS
#include <sof/lib/dai.h>
#include <zephyr/drivers/dai.h>
#include <zephyr/drivers/dma.h>
#endif

#if CONFIG_SOF_SHELL_DAI_LIST

__cold static const char *zephyr_dai_type_str(int t)
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

__cold static int cmd_sof_dai_list(const struct shell *sh,
				   size_t argc, char *argv[])
{
	const struct device **list;
	size_t count = 0;
	bool verbose = (argc >= 2 && strcmp(argv[1], "-v") == 0);
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

		if (!verbose)
			continue;

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

	return 0;
}

#endif /* CONFIG_SOF_SHELL_DAI_LIST */

#if CONFIG_SOF_SHELL_DMA_STATUS

__cold static const char *dma_dir_str(enum dma_channel_direction d)
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

__cold static void dma_print_one(const struct shell *sh, struct sof_dma *dma,
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
			    "Usage: sof dma status <dma_idx> [chan]  (omit chan to walk all)");
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

#if CONFIG_SOF_SHELL_DAI_LIST
SHELL_SUBCMD_ADD((sof), dai_list, NULL,
		 "List all registered DAIs (name, type, channels, rate)\n"
		 "  [-v]  also show TX/RX fifo address, depth, hs, stream\n",
		 cmd_sof_dai_list, 1, 1);
#endif

#if CONFIG_SOF_SHELL_DMA_STATUS
SHELL_SUBCMD_ADD((sof), dma_status, NULL,
		 "List DMA controllers, or per-channel status: "
		 "[dma_idx] [chan]\n",
		 cmd_sof_dma_status, 1, 2);
#endif

#if CONFIG_SOF_SHELL_KCTL_LIST

/*
 * Best-effort decoded driver name. Module-adapter components share
 * SOF_COMP_MODULE_ADAPTER for drv->type, so the only stable per-module
 * label available in firmware is the UUID name string from the trace
 * context (which carries the same name printed by the LDC tool).
 */
__cold static const char *kctl_drv_name(const struct comp_dev *cd)
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
__cold static const char *kctl_drv_kind(const char *name)
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
		    "sof-ctl on the host, or 'sof module status' for raw state.");

	return 0;
}

#endif /* CONFIG_SOF_SHELL_KCTL_LIST */

#if CONFIG_SOF_SHELL_KCTL_LIST
SHELL_SUBCMD_ADD((sof), kctl_list, NULL,
		 "List components and their decoded module name / kind\n",
		 cmd_sof_kctl_list, 0, 0);
#endif

#if CONFIG_SOF_SHELL_HEAP_USAGE || CONFIG_SOF_SHELL_MODULE_STATUS || CONFIG_SOF_SHELL_MODULE_LIST
SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_module_heap,
#if CONFIG_SOF_SHELL_HEAP_USAGE
	SHELL_CMD(usage, NULL,
		  "Print heap memory usage of each module\n",
		  cmd_sof_module_heap_usage),
#endif
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_module,
#if CONFIG_SOF_SHELL_HEAP_USAGE
	SHELL_CMD(heap, &sof_cmd_module_heap,
		  "Module heap commands\n", NULL),
#endif
#if CONFIG_SOF_SHELL_MODULE_STATUS
	SHELL_CMD(status, NULL,
		  "Print status of all active components\n",
		  cmd_sof_module_status),
#endif
#if CONFIG_SOF_SHELL_MODULE_LIST
	SHELL_CMD_ARG(list, NULL,
		      "List all available modules (name, inst, cpc, text, bss)\n"
		      "  [-v]  also show uuid, affinity, cps, ibs, obs\n",
		      cmd_sof_module_list, 1, 1),
#endif
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_PIPELINE_STATUS || CONFIG_SOF_SHELL_PIPELINE_OPS
SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_pipeline,
#if CONFIG_SOF_SHELL_PIPELINE_STATUS
	SHELL_CMD(status, NULL,
		  "Print status of all active pipelines\n",
		  cmd_sof_pipeline_status),
#endif
	SHELL_CMD(list, NULL,
		  "List all active audio pipelines\n",
		  cmd_sof_pipeline_list),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_PIPELINE_OPS
SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_ppl,
	SHELL_CMD_ARG(create, NULL,
		  "Create IPC4 pipeline: <ppl_id> [priority=0] [pages=2] [core=0] [lp=0]\n",
		  cmd_sof_ppl_create, 2, 4),
	SHELL_CMD_ARG(delete, NULL,
		  "Delete IPC4 pipeline: <ppl_id>\n",
		  cmd_sof_ppl_delete, 2, 0),
	SHELL_CMD_ARG(state, NULL,
		  "Set IPC4 pipeline state: <ppl_id> <running|paused|reset>\n",
		  cmd_sof_ppl_state, 3, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_mod,
	SHELL_CMD_ARG(init, NULL,
		  "Instantiate module: <mod_id> <inst_id> <ppl_id> [core=0] [dp=0]\n",
		  cmd_sof_mod_init, 4, 2),
	SHELL_CMD_ARG(delete, NULL,
		  "Delete module instance: <mod_id> <inst_id>\n",
		  cmd_sof_mod_delete, 3, 0),
	SHELL_CMD_ARG(bind, NULL,
		  "Bind two module instances: <src_mod> <src_inst> <dst_mod> <dst_inst>"
		  " [src_q=0] [dst_q=0]\n",
		  cmd_sof_mod_bind, 5, 2),
	SHELL_CMD_ARG(unbind, NULL,
		  "Unbind two module instances: <src_mod> <src_inst> <dst_mod> <dst_inst>"
		  " [src_q=0] [dst_q=0]\n",
		  cmd_sof_mod_unbind, 5, 2),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_BUFFER_INFO
SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_buffer,
	SHELL_CMD(list, NULL,
		  "List all audio buffers (id, source/sink, fill, format)\n",
		  cmd_sof_buffer_list),
	SHELL_CMD_ARG(info, NULL,
		  "Detailed info for a single buffer: <buffer_id>\n",
		  cmd_sof_buffer_info, 2, 0),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_DAI_LIST
SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_dai,
	SHELL_CMD_ARG(list, NULL,
		      "List all registered DAIs (name, type, channels, rate)\n"
		      "  [-v]  also show TX/RX fifo address, depth, hs, stream\n",
		      cmd_sof_dai_list, 1, 1),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_DMA_STATUS
SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_dma,
	SHELL_CMD_ARG(status, NULL,
		  "List DMA controllers, or per-channel status: "
		  "[dma_idx] [chan]\n",
		  cmd_sof_dma_status, 1, 2),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_KCTL_LIST
SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmd_kctl,
	SHELL_CMD(list, NULL,
		  "List components and their decoded module name / kind\n",
		  cmd_sof_kctl_list),
	SHELL_SUBCMD_SET_END
);
#endif

#if CONFIG_SOF_SHELL_HEAP_USAGE || CONFIG_SOF_SHELL_MODULE_STATUS || CONFIG_SOF_SHELL_MODULE_LIST
SHELL_SUBCMD_ADD((sof), module, &sof_cmd_module,
		 "Module commands\n", NULL, 0, 0);
#endif

#if CONFIG_SOF_SHELL_PIPELINE_STATUS || CONFIG_SOF_SHELL_PIPELINE_OPS
SHELL_SUBCMD_ADD((sof), pipeline, &sof_cmd_pipeline,
		 "Pipeline commands\n", NULL, 0, 0);
#endif

#if CONFIG_SOF_SHELL_PIPELINE_OPS
SHELL_SUBCMD_ADD((sof), ppl, &sof_cmd_ppl,
		 "Pipeline operation commands\n", NULL, 0, 0);
SHELL_SUBCMD_ADD((sof), mod, &sof_cmd_mod,
		 "Module operation commands\n", NULL, 0, 0);
#endif

#if CONFIG_SOF_SHELL_BUFFER_INFO
SHELL_SUBCMD_ADD((sof), buffer, &sof_cmd_buffer,
		 "Buffer commands\n", NULL, 0, 0);
#endif

#if CONFIG_SOF_SHELL_DAI_LIST
SHELL_SUBCMD_ADD((sof), dai, &sof_cmd_dai,
		 "DAI commands\n", NULL, 0, 0);
#endif

#if CONFIG_SOF_SHELL_DMA_STATUS
SHELL_SUBCMD_ADD((sof), dma, &sof_cmd_dma,
		 "DMA commands\n", NULL, 0, 0);
#endif

#if CONFIG_SOF_SHELL_KCTL_LIST
SHELL_SUBCMD_ADD((sof), kctl, &sof_cmd_kctl,
		 "Kernel-control commands\n", NULL, 0, 0);
#endif

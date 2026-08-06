// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

/*
 * System call implementations for the SOF kernel (RTOS/infrastructure) shell
 * commands. The z_impl_* functions run in supervisor context and forward to
 * the underlying privileged accessors. The z_vrfy_* handlers validate
 * user-supplied pointers before dispatching when the shell thread runs in
 * user mode (CONFIG_USERSPACE).
 */

#include <sof/ipc/common.h>
#include <sof/lib/cpu.h>
#include <rtos/clk.h>
#include <rtos/sof.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <zephyr/kernel.h>
#include <sof/sof_shell_syscall.h>

#if CONFIG_SOF_SHELL_SCHED_INFO
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <sof/list.h>
#endif

#if CONFIG_SOF_SHELL_LOG_INFO
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_core.h>
#include <string.h>
#endif

#if (CONFIG_SOF_SHELL_LLEXT_LIST || CONFIG_SOF_SHELL_LLEXT_PURGE || \
     CONFIG_SOF_SHELL_LLEXT_CTOR || CONFIG_SOF_SHELL_LLEXT_CALL) && \
    CONFIG_LIBRARY_MANAGER
#include <sof/lib_manager.h>
#include <sof/audio/component.h>
#include <string.h>
#ifndef _SHELL_MOD_PAGE_SZ
#ifdef CONFIG_MM_DRV_PAGE_SIZE
#define _SHELL_MOD_PAGE_SZ CONFIG_MM_DRV_PAGE_SIZE
#else
#define _SHELL_MOD_PAGE_SZ 4096
#endif
#endif
#endif

#if (CONFIG_SOF_SHELL_LLEXT_CTOR || CONFIG_SOF_SHELL_LLEXT_CALL) && \
    CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER
#include <zephyr/llext/llext.h>
#include <sof/llext_manager.h>
#endif

#if CONFIG_SOF_SHELL_LLEXT_CALL && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER
#include <zephyr/llext/buf_loader.h>
#endif

#if CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB
#include <zephyr/devicetree.h>
#include <zephyr/kernel/mm.h>
#include <zephyr/drivers/mm/system_mm.h>
#include <zephyr/sys/util.h>

/*
 * Lightweight wrappers around the Intel ADSP MTL TLB MMIO table. Mirrors
 * mm_drv_intel_adsp.h without pulling in the driver-internal header. These
 * accesses are privileged (MMIO and the memory-management driver) and so live
 * here behind system calls; the shell only consumes the decoded snapshot.
 */
#define _SHELL_TLB_NODE       DT_NODELABEL(tlb)
#define _SHELL_TLB_BASE       ((volatile uint16_t *)(uintptr_t)DT_REG_ADDR(_SHELL_TLB_NODE))
#define _SHELL_PADDR_SIZE     DT_PROP(_SHELL_TLB_NODE, paddr_size)
#define _SHELL_TLB_ENTRY_NUM  BIT(_SHELL_PADDR_SIZE)
#define _SHELL_PADDR_MASK     (_SHELL_TLB_ENTRY_NUM - 1)
#define _SHELL_ENABLE_BIT     ((uint16_t)BIT(_SHELL_PADDR_SIZE))

/*
 * Base physical address for the HPSRAM region (mirrors TLB_PHYS_BASE in the
 * driver).
 */
#define _SHELL_PHYS_BASE \
	(((CONFIG_KERNEL_VM_BASE / CONFIG_MM_DRV_PAGE_SIZE) & ~_SHELL_PADDR_MASK) * \
	 CONFIG_MM_DRV_PAGE_SIZE)
#endif /* CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB */

void z_impl_sof_shell_ipc_stats_get(struct ipc_stats *out)
{
	ipc_stats_get(out);
}

void z_impl_sof_shell_ipc_stats_reset(void)
{
	ipc_stats_reset();
}

void z_impl_sof_shell_core_status_get(struct sof_shell_core_status *out)
{
	unsigned int i;

	out->core_count = CONFIG_CORE_COUNT;
	out->current = cpu_get_id();
	for (i = 0; i < CONFIG_CORE_COUNT; i++)
		out->enabled[i] = cpu_is_core_enabled(i) ? 1 : 0;
}

#if CONFIG_SOF_SHELL_CLOCK_STATUS
void z_impl_sof_shell_clock_status_get(struct sof_shell_clock_status *out)
{
	struct clock_info *clocks = clocks_get();
	unsigned int i;

	if (!clocks) {
		out->valid = 0;
		out->num_clocks = 0;
		return;
	}

	out->valid = 1;
	out->num_clocks = NUM_CLOCKS;
	for (i = 0; i < NUM_CLOCKS && i < CONFIG_CORE_COUNT; i++)
		out->freq_hz[i] = clocks[i].freqs[clocks[i].current_freq_idx].freq;
}
#endif

#if CONFIG_SOF_SHELL_SCHED_INFO
static void sof_shell_sched_cb(struct task *task, void *_ctx)
{
	struct sof_shell_sched_snapshot *out = _ctx;
	struct sof_shell_sched_task *e;

	if (out->count >= SOF_SHELL_SCHED_MAX_TASKS)
		return;

	e = &out->tasks[out->count++];
	e->sch_type = task->sch ? task->sch->type : 0;
	e->core = task->core;
	e->priority = task->priority;
	e->state = task->state;
	e->flags = task->flags;
	e->cycles_cnt = task->cycles_cnt;
	e->cycles_sum = task->cycles_sum;
	e->cycles_max = task->cycles_max;
	e->uid = (uintptr_t)task->uid;
	e->data = (uintptr_t)task->data;
}

void z_impl_sof_shell_sched_snapshot_get(struct sof_shell_sched_snapshot *out)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	out->count = 0;
	out->no_schedulers = 0;

	if (!schedulers) {
		out->no_schedulers = 1;
		return;
	}

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (!sch->ops->scheduler_dump_tasks)
			continue;
		sch->ops->scheduler_dump_tasks(sch->data, sof_shell_sched_cb, out);
	}
}
#endif /* CONFIG_SOF_SHELL_SCHED_INFO */

#if CONFIG_SOF_SHELL_LOG_INFO
void z_impl_sof_shell_log_status_get(struct sof_shell_log_status *out)
{
	int n = log_backend_count_get();
	int i;

	out->backend_count = n;
	out->source_count = log_src_cnt_get(Z_LOG_LOCAL_DOMAIN_ID);
	out->filled = 0;

	for (i = 0; i < n && out->filled < SOF_SHELL_LOG_MAX_BACKENDS; i++) {
		const struct log_backend *be = log_backend_get(i);
		struct sof_shell_log_backend *e;
		const char *name;

		if (!be)
			continue;

		e = &out->backends[out->filled++];
		e->id = log_backend_id_get(be);
		e->active = log_backend_is_active(be) ? 1 : 0;
		name = be->name ? be->name : "?";
		strncpy(e->name, name, SOF_SHELL_LOG_NAME_MAX - 1);
		e->name[SOF_SHELL_LOG_NAME_MAX - 1] = '\0';
	}
}
#endif /* CONFIG_SOF_SHELL_LOG_INFO */

#if CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB
void z_impl_sof_shell_tlb_meta_get(struct sof_shell_tlb_meta *out)
{
	volatile uint16_t *tlb = _SHELL_TLB_BASE;
	uint32_t total = _SHELL_TLB_ENTRY_NUM;
	const struct sys_mm_drv_region *regions, *r;
	uint32_t enabled = 0;
	uint32_t i;

	for (i = 0; i < total; i++) {
		if (tlb[i] & _SHELL_ENABLE_BIT)
			enabled++;
	}

	out->vm_base = CONFIG_KERNEL_VM_BASE;
	out->page_size = CONFIG_MM_DRV_PAGE_SIZE;
	out->total_entries = total;
	out->enabled_entries = enabled;
	out->tlb_mmio_base = (uint32_t)(uintptr_t)_SHELL_TLB_BASE;
	out->phys_base = (uint32_t)_SHELL_PHYS_BASE;
	out->paddr_size = _SHELL_PADDR_SIZE;
	out->exec_bit_idx = DT_PROP(_SHELL_TLB_NODE, exec_bit_idx);
	out->write_bit_idx = DT_PROP(_SHELL_TLB_NODE, write_bit_idx);
	out->region_count = 0;
	out->regions_truncated = 0;

	regions = sys_mm_drv_query_memory_regions();
	if (regions) {
		SYS_MM_DRV_MEMORY_REGION_FOREACH(regions, r) {
			struct sof_shell_mm_region *e;

			if (out->region_count >= SOF_SHELL_TLB_MAX_REGIONS) {
				out->regions_truncated = 1;
				break;
			}
			e = &out->regions[out->region_count++];
			e->addr = (uint32_t)(uintptr_t)r->addr;
			e->size = (uint32_t)r->size;
			e->attr = (uint32_t)r->attr;
		}
		sys_mm_drv_query_memory_regions_free(regions);
	}
}

uint32_t z_impl_sof_shell_tlb_entries_get(uint32_t start, uint32_t count,
					  uint16_t *out)
{
	volatile uint16_t *tlb = _SHELL_TLB_BASE;
	uint32_t total = _SHELL_TLB_ENTRY_NUM;
	uint32_t i;

	if (start >= total)
		return 0;
	if (count > total - start)
		count = total - start;

	for (i = 0; i < count; i++)
		out[i] = tlb[start + i];

	return count;
}
#endif /* CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB */

#if CONFIG_SOF_SHELL_CORE_POWER
int z_impl_sof_shell_core_is_enabled(uint32_t id)
{
	return cpu_is_core_enabled((int)id) ? 1 : 0;
}

int z_impl_sof_shell_core_enable(uint32_t id)
{
	return cpu_enable_core((int)id);
}

void z_impl_sof_shell_core_disable(uint32_t id)
{
	cpu_disable_core((int)id);
}
#endif /* CONFIG_SOF_SHELL_CORE_POWER */

void z_impl_sof_shell_inject_sched_gap(uint32_t block_time_us)
{
	struct ll_schedule_domain *domain = sof_get()->platform_timer_domain;

	domain_block(domain);
	k_busy_wait(block_time_us);
	domain_unblock(domain);
}

#if CONFIG_SOF_SHELL_LLEXT_LIST && CONFIG_LIBRARY_MANAGER
void z_impl_sof_shell_llext_list_get(struct sof_shell_llext_list *out)
{
	struct ext_library *ext_lib = ext_lib_get();
	int lib_id;

	out->enabled = 1;
	out->count = 0;

	for (lib_id = 1; lib_id < LIB_MANAGER_MAX_LIBS &&
	     out->count < SOF_SHELL_LLEXT_MAX_LIBS; lib_id++) {
		const struct lib_manager_mod_ctx *ctx = ext_lib->desc[lib_id];
		const struct sof_man_fw_desc *desc;
		struct sof_shell_llext_lib *lib;

		if (!ctx || !ctx->base_addr)
			continue;

		desc = (const struct sof_man_fw_desc *)
			((const uint8_t *)ctx->base_addr + SOF_MAN_ELF_TEXT_OFFSET);

		lib = &out->libs[out->count++];
		lib->lib_id = lib_id;
		lib->base_addr = (uint32_t)(uintptr_t)ctx->base_addr;
		lib->store_bytes = desc->header.preload_page_count *
				   (uint32_t)_SHELL_MOD_PAGE_SZ;
		lib->manifest_mods = desc->header.num_module_entries;
		lib->elf_files = ctx->n_mod;
		lib->mod_count = 0;

#if CONFIG_LLEXT
		if (ctx->mod) {
			unsigned int i;

			for (i = 0; i < ctx->n_mod &&
			     lib->mod_count < SOF_SHELL_LLEXT_MAX_MODS; i++) {
				const struct lib_manager_module *m = ctx->mod + i;
				struct sof_shell_llext_mod *me =
					&lib->mods[lib->mod_count++];
				const uint8_t *nm;

				me->mapped = m->mapped ? 1 : 0;
				me->use = m->llext ? (int)m->llext->use_count : 0;
				me->dep = m->n_dependent;

				if (m->mod_manifest) {
					nm = m->mod_manifest->module.name;
				} else {
					const struct sof_man_module *mm =
						(const struct sof_man_module *)
						((const uint8_t *)desc +
						 SOF_MAN_MODULE_OFFSET(m->start_idx));
					nm = mm->name;
				}
				memcpy(me->name, nm, SOF_MAN_MOD_NAME_LEN);
				me->name[SOF_MAN_MOD_NAME_LEN] = '\0';
			}
		}
#endif /* CONFIG_LLEXT */
	}
}
#endif /* CONFIG_SOF_SHELL_LLEXT_LIST && CONFIG_LIBRARY_MANAGER */

#if CONFIG_SOF_SHELL_LLEXT_PURGE && CONFIG_LIBRARY_MANAGER
int z_impl_sof_shell_llext_purge(uint32_t lib_id)
{
	return lib_manager_purge_library(lib_id);
}
#endif /* CONFIG_SOF_SHELL_LLEXT_PURGE && CONFIG_LIBRARY_MANAGER */

#if CONFIG_SOF_SHELL_LLEXT_CTOR && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER
int z_impl_sof_shell_llext_ctor_dtor(uint32_t lib_id, uint32_t is_ctor,
				     struct sof_shell_llext_op_result *out)
{
	struct ext_library *ext_lib = ext_lib_get();
	struct lib_manager_mod_ctx *ctx = ext_lib->desc[lib_id];
	unsigned int i;
	int ret = 0;

	out->status = 0;
	out->count = 0;

	if (!ctx || !ctx->base_addr || !ctx->mod) {
		out->status = -ENOENT;
		return -ENOENT;
	}

	for (i = 0; i < ctx->n_mod && out->count < SOF_SHELL_LLEXT_MAX_MODS; i++) {
		struct lib_manager_module *m = &ctx->mod[i];
		uint32_t module_id = (lib_id << LIB_MANAGER_LIB_ID_SHIFT) | m->start_idx;
		struct sof_shell_llext_op_mod *me = &out->mods[out->count++];

		me->flag = 0;
		me->addr = 0;
		me->name[0] = '\0';

		if (is_ctor) {
			struct comp_ipc_config fake_ipc = { .id = module_id };
			uintptr_t entry = llext_manager_allocate_module(&fake_ipc, NULL);

			if (!entry) {
				me->ret = -ENOMEM;
				out->status = -ENOMEM;
				return -ENOMEM;
			}

			if (!m->llext) {
				me->ret = -ENODEV;
				out->status = -ENODEV;
				return -ENODEV;
			}

			if (m->llext->name[0]) {
				strncpy(me->name, m->llext->name,
					SOF_SHELL_LLEXT_NAME_MAX - 1);
				me->name[SOF_SHELL_LLEXT_NAME_MAX - 1] = '\0';
			}

			ret = llext_bringup(m->llext);
		} else {
			if (!m->llext) {
				me->ret = -ENODEV;
				out->status = -ENODEV;
				return -ENODEV;
			}

			if (m->llext->name[0]) {
				strncpy(me->name, m->llext->name,
					SOF_SHELL_LLEXT_NAME_MAX - 1);
				me->name[SOF_SHELL_LLEXT_NAME_MAX - 1] = '\0';
			}

			ret = llext_teardown(m->llext);
			if (ret < 0) {
				me->ret = ret;
				out->status = ret;
				return ret;
			}

			ret = llext_manager_free_module(module_id);
		}

		me->ret = ret;
		if (ret < 0) {
			out->status = ret;
			return ret;
		}
	}

	return ret;
}
#endif /* CONFIG_SOF_SHELL_LLEXT_CTOR && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER */

#if CONFIG_SOF_SHELL_LLEXT_CALL && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER
int z_impl_sof_shell_llext_call(uint32_t lib_id, const char *sym_name,
				struct sof_shell_llext_op_result *out)
{
	struct ext_library *ext_lib = ext_lib_get();
	struct lib_manager_mod_ctx *ctx = ext_lib->desc[lib_id];
	unsigned int i;
	int found = 0;

	out->status = 0;
	out->count = 0;

	if (!ctx || !ctx->base_addr || !ctx->mod) {
		out->status = -ENOENT;
		return -ENOENT;
	}

	for (i = 0; i < ctx->n_mod && out->count < SOF_SHELL_LLEXT_MAX_MODS; i++) {
		struct lib_manager_module *m = &ctx->mod[i];
		struct sof_shell_llext_op_mod *me = &out->mods[out->count++];
		const void *fn_addr = NULL;

		me->ret = 0;
		me->flag = 0;
		me->addr = 0;
		me->name[0] = '\0';

		if (!m->llext) {
			me->ret = -ENODEV;
			continue;
		}

		if (m->llext->name[0]) {
			strncpy(me->name, m->llext->name, SOF_SHELL_LLEXT_NAME_MAX - 1);
			me->name[SOF_SHELL_LLEXT_NAME_MAX - 1] = '\0';
		}

		fn_addr = llext_find_sym(&m->llext->sym_tab, sym_name);
		if (!fn_addr)
			fn_addr = llext_find_sym(&m->llext->exp_tab, sym_name);

		/*
		 * Fallback to the raw ELF symbol table in the persistent DRAM
		 * buffer (covers cases where sym_tab was freed or exp_tab
		 * metadata is insufficient).
		 */
		if (!fn_addr && m->ebl) {
			const struct llext_loader *raw_ldr = &m->ebl->loader;
			const elf_shdr_t *symtab_hdr = &raw_ldr->sects[LLEXT_MEM_SYMTAB];
			const elf_shdr_t *strtab_hdr = &raw_ldr->sects[LLEXT_MEM_STRTAB];

			if (symtab_hdr->sh_size && strtab_hdr->sh_size) {
				const elf_sym_t *syms = (const elf_sym_t *)
					(m->ebl->buf + symtab_hdr->sh_offset);
				const char *strs = (const char *)
					(m->ebl->buf + strtab_hdr->sh_offset);
				uint32_t sym_cnt = symtab_hdr->sh_size / sizeof(elf_sym_t);
				uint32_t k;

				for (k = 0; k < sym_cnt; k++) {
					uint16_t shndx;
					const elf_shdr_t *sect;

					if (ELF_ST_TYPE(syms[k].st_info) != STT_FUNC)
						continue;
					if (strcmp(strs + syms[k].st_name, sym_name) != 0)
						continue;

					shndx = syms[k].st_shndx;
					if (shndx >= m->llext->sect_cnt)
						break;

					sect = m->llext->sect_hdrs + shndx;
					fn_addr = (const void *)(m->ebl->buf +
						sect->sh_offset + syms[k].st_value);
					break;
				}
			}
		}

		if (!fn_addr) {
			me->ret = -ENOENT;
			continue;
		}

		me->flag = 1;
		me->addr = (uintptr_t)fn_addr;

		((void (*)(void))fn_addr)();

		found++;
	}

	out->status = found ? 0 : -ENOENT;
	return out->status;
}
#endif /* CONFIG_SOF_SHELL_LLEXT_CALL && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER */

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>

void z_vrfy_sof_shell_ipc_stats_get(struct ipc_stats *out)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));
	z_impl_sof_shell_ipc_stats_get(out);
}
#include <zephyr/syscalls/sof_shell_ipc_stats_get_mrsh.c>

void z_vrfy_sof_shell_ipc_stats_reset(void)
{
	z_impl_sof_shell_ipc_stats_reset();
}
#include <zephyr/syscalls/sof_shell_ipc_stats_reset_mrsh.c>

void z_vrfy_sof_shell_core_status_get(struct sof_shell_core_status *out)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));
	z_impl_sof_shell_core_status_get(out);
}
#include <zephyr/syscalls/sof_shell_core_status_get_mrsh.c>

#if CONFIG_SOF_SHELL_CLOCK_STATUS
void z_vrfy_sof_shell_clock_status_get(struct sof_shell_clock_status *out)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));
	z_impl_sof_shell_clock_status_get(out);
}
#include <zephyr/syscalls/sof_shell_clock_status_get_mrsh.c>
#endif

#if CONFIG_SOF_SHELL_SCHED_INFO
void z_vrfy_sof_shell_sched_snapshot_get(struct sof_shell_sched_snapshot *out)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));
	z_impl_sof_shell_sched_snapshot_get(out);
}
#include <zephyr/syscalls/sof_shell_sched_snapshot_get_mrsh.c>
#endif /* CONFIG_SOF_SHELL_SCHED_INFO */

#if CONFIG_SOF_SHELL_LOG_INFO
void z_vrfy_sof_shell_log_status_get(struct sof_shell_log_status *out)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));
	z_impl_sof_shell_log_status_get(out);
}
#include <zephyr/syscalls/sof_shell_log_status_get_mrsh.c>
#endif /* CONFIG_SOF_SHELL_LOG_INFO */

#if CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB
void z_vrfy_sof_shell_tlb_meta_get(struct sof_shell_tlb_meta *out)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));
	z_impl_sof_shell_tlb_meta_get(out);
}
#include <zephyr/syscalls/sof_shell_tlb_meta_get_mrsh.c>

uint32_t z_vrfy_sof_shell_tlb_entries_get(uint32_t start, uint32_t count,
					  uint16_t *out)
{
	K_OOPS(K_SYSCALL_MEMORY_ARRAY_WRITE(out, count, sizeof(uint16_t)));
	return z_impl_sof_shell_tlb_entries_get(start, count, out);
}
#include <zephyr/syscalls/sof_shell_tlb_entries_get_mrsh.c>
#endif /* CONFIG_SOF_SHELL_MMU_DBG && CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB */

#if CONFIG_SOF_SHELL_CORE_POWER
int z_vrfy_sof_shell_core_is_enabled(uint32_t id)
{
	K_OOPS(K_SYSCALL_VERIFY_MSG(id < CONFIG_CORE_COUNT, "invalid core id"));
	return z_impl_sof_shell_core_is_enabled(id);
}
#include <zephyr/syscalls/sof_shell_core_is_enabled_mrsh.c>

int z_vrfy_sof_shell_core_enable(uint32_t id)
{
	K_OOPS(K_SYSCALL_VERIFY_MSG(id >= 1 && id < CONFIG_CORE_COUNT,
				   "invalid core id"));
	return z_impl_sof_shell_core_enable(id);
}
#include <zephyr/syscalls/sof_shell_core_enable_mrsh.c>

void z_vrfy_sof_shell_core_disable(uint32_t id)
{
	K_OOPS(K_SYSCALL_VERIFY_MSG(id >= 1 && id < CONFIG_CORE_COUNT,
				   "invalid core id"));
	z_impl_sof_shell_core_disable(id);
}
#include <zephyr/syscalls/sof_shell_core_disable_mrsh.c>
#endif /* CONFIG_SOF_SHELL_CORE_POWER */

void z_vrfy_sof_shell_inject_sched_gap(uint32_t block_time_us)
{
	z_impl_sof_shell_inject_sched_gap(block_time_us);
}
#include <zephyr/syscalls/sof_shell_inject_sched_gap_mrsh.c>

#if CONFIG_SOF_SHELL_LLEXT_LIST && CONFIG_LIBRARY_MANAGER
void z_vrfy_sof_shell_llext_list_get(struct sof_shell_llext_list *out)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));
	z_impl_sof_shell_llext_list_get(out);
}
#include <zephyr/syscalls/sof_shell_llext_list_get_mrsh.c>
#endif /* CONFIG_SOF_SHELL_LLEXT_LIST && CONFIG_LIBRARY_MANAGER */

#if CONFIG_SOF_SHELL_LLEXT_PURGE && CONFIG_LIBRARY_MANAGER
int z_vrfy_sof_shell_llext_purge(uint32_t lib_id)
{
	K_OOPS(K_SYSCALL_VERIFY_MSG(lib_id >= 1 && lib_id < LIB_MANAGER_MAX_LIBS,
				   "invalid lib_id"));
	return z_impl_sof_shell_llext_purge(lib_id);
}
#include <zephyr/syscalls/sof_shell_llext_purge_mrsh.c>
#endif /* CONFIG_SOF_SHELL_LLEXT_PURGE && CONFIG_LIBRARY_MANAGER */

#if CONFIG_SOF_SHELL_LLEXT_CTOR && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER
int z_vrfy_sof_shell_llext_ctor_dtor(uint32_t lib_id, uint32_t is_ctor,
				     struct sof_shell_llext_op_result *out)
{
	K_OOPS(K_SYSCALL_VERIFY_MSG(lib_id >= 1 && lib_id < LIB_MANAGER_MAX_LIBS,
				   "invalid lib_id"));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));
	return z_impl_sof_shell_llext_ctor_dtor(lib_id, is_ctor, out);
}
#include <zephyr/syscalls/sof_shell_llext_ctor_dtor_mrsh.c>
#endif /* CONFIG_SOF_SHELL_LLEXT_CTOR && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER */

#if CONFIG_SOF_SHELL_LLEXT_CALL && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER
int z_vrfy_sof_shell_llext_call(uint32_t lib_id, const char *sym_name,
				struct sof_shell_llext_op_result *out)
{
	char kname[SOF_SHELL_LLEXT_SYM_MAX];
	int len;

	K_OOPS(K_SYSCALL_VERIFY_MSG(lib_id >= 1 && lib_id < LIB_MANAGER_MAX_LIBS,
				   "invalid lib_id"));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(out, sizeof(*out)));

	len = k_usermode_string_copy(kname, (char *)sym_name, sizeof(kname));
	K_OOPS(K_SYSCALL_VERIFY_MSG(len == 0, "symbol name too long or invalid"));

	return z_impl_sof_shell_llext_call(lib_id, kname, out);
}
#include <zephyr/syscalls/sof_shell_llext_call_mrsh.c>
#endif /* CONFIG_SOF_SHELL_LLEXT_CALL && CONFIG_LLEXT && CONFIG_LIBRARY_MANAGER */
#endif /* CONFIG_USERSPACE */

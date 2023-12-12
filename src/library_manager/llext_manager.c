// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
//         Pawel Dobrowolski<pawelx.dobrowolski@intel.com>

/*
 * Dynamic module loading functions.
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/common.h>
#include <sof/compiler_attributes.h>
#include <sof/ipc/topology.h>

#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/lib_manager.h>
#include <sof/llext_manager.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/modules.h>

#include <zephyr/cache.h>
#include <zephyr/drivers/mm/system_mm.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(lib_manager, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx lib_manager_tr;

#define PAGE_SZ		CONFIG_MM_DRV_PAGE_SIZE

static int llext_manager_align_map(void __sparse_cache *vma, size_t size, uint32_t flags)
{
	size_t pre_pad_size = (uintptr_t)vma & (PAGE_SZ - 1);
	void *aligned_vma = (__sparse_force uint8_t *)vma - pre_pad_size;

	return sys_mm_drv_map_region(aligned_vma, POINTER_TO_UINT(NULL),
				     ALIGN_UP(pre_pad_size + size, PAGE_SZ), flags);
}

static int llext_manager_align_unmap(void __sparse_cache *vma, size_t size)
{
	size_t pre_pad_size = (uintptr_t)vma & (PAGE_SZ - 1);
	void *aligned_vma = (__sparse_force uint8_t *)vma - pre_pad_size;

	return sys_mm_drv_unmap_region(aligned_vma, ALIGN_UP(pre_pad_size + size, PAGE_SZ));
}

static int llext_manager_load_data_from_storage(void __sparse_cache *vma, void *s_addr,
						size_t size, uint32_t flags)
{
	int ret = llext_manager_align_map(vma, size, flags);

	if (ret < 0) {
		tr_err(&lib_manager_tr, "cannot map %u of %p", size, (__sparse_force void *)vma);
		return ret;
	}

	ret = memcpy_s((__sparse_force void *)vma, size, s_addr, size);
	if (ret < 0)
		return ret;

	/* Some data can be accessed as uncached, in fact that's the default */
	/* Both D- and I-caches have been invalidated */
	dcache_writeback_region(vma, size);

	/* TODO: Change attributes for memory to FLAGS  */
	return 0;
}

static int llext_manager_load_module(uint32_t module_id, struct sof_man_module *mod,
				     struct sof_man_fw_desc *desc)
{
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	uint8_t *load_base = (uint8_t *)ctx->desc;
	void __sparse_cache *va_base_text = (void __sparse_cache *)
		mod->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr;
	void *src_txt = (void *)(load_base + mod->segment[SOF_MAN_SEGMENT_TEXT].file_offset);
	size_t st_text_size = ctx->segment_size[SOF_MAN_SEGMENT_TEXT];
	void __sparse_cache *va_base_rodata = (void __sparse_cache *)
		mod->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr;
	void *src_rodata = (void *)(load_base +
				    mod->segment[SOF_MAN_SEGMENT_RODATA].file_offset);
	size_t st_rodata_size = ctx->segment_size[SOF_MAN_SEGMENT_RODATA];
	int ret;

	/* Copy Code */
	ret = llext_manager_load_data_from_storage(va_base_text, src_txt, st_text_size,
						   SYS_MM_MEM_PERM_RW | SYS_MM_MEM_PERM_EXEC);
	if (ret < 0)
		return ret;

	/* Copy RODATA */
	ret = llext_manager_load_data_from_storage(va_base_rodata, src_rodata,
						   st_rodata_size, SYS_MM_MEM_PERM_RW);
	if (ret < 0)
		goto e_text;

	return 0;

e_text:
	llext_manager_align_unmap(va_base_text, st_text_size);

	return ret;
}

static int llext_manager_unload_module(uint32_t module_id, struct sof_man_module *mod,
				       struct sof_man_fw_desc *desc)
{
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	void __sparse_cache *va_base_text = (void __sparse_cache *)
		mod->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr;
	size_t st_text_size = ctx->segment_size[SOF_MAN_SEGMENT_TEXT];
	void __sparse_cache *va_base_rodata = (void __sparse_cache *)
		mod->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr;
	size_t st_rodata_size = ctx->segment_size[SOF_MAN_SEGMENT_RODATA];
	int ret;

	ret = llext_manager_align_unmap(va_base_text, st_text_size);
	if (ret < 0)
		return ret;

	return llext_manager_align_unmap(va_base_rodata, st_rodata_size);
}

static void __sparse_cache *llext_manager_get_bss_address(uint32_t module_id,
							  struct sof_man_module *mod)
{
	return (void __sparse_cache *)mod->segment[SOF_MAN_SEGMENT_BSS].v_base_addr;
}

static int llext_manager_allocate_module_bss(uint32_t module_id,
					     uint32_t is_pages, struct sof_man_module *mod)
{
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	size_t bss_size = ctx->segment_size[SOF_MAN_SEGMENT_BSS];
	void __sparse_cache *va_base = llext_manager_get_bss_address(module_id, mod);

	if (is_pages * PAGE_SZ > bss_size) {
		tr_err(&lib_manager_tr,
		       "llext_manager_allocate_module_bss(): invalid is_pages: %u, required: %u",
		       is_pages, bss_size / PAGE_SZ);
		return -ENOMEM;
	}

	/* Map bss memory and clear it. */
	if (llext_manager_align_map(va_base, bss_size, SYS_MM_MEM_PERM_RW) < 0)
		return -ENOMEM;

	memset((__sparse_force void *)va_base, 0, bss_size);

	return 0;
}

static int llext_manager_free_module_bss(uint32_t module_id,
					 struct sof_man_module *mod)
{
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	size_t bss_size = ctx->segment_size[SOF_MAN_SEGMENT_BSS];
	void __sparse_cache *va_base = llext_manager_get_bss_address(module_id, mod);

	/* Unmap bss memory. */
	return llext_manager_align_unmap(va_base, bss_size);
}

uint32_t llext_manager_allocate_module(const struct comp_driver *drv,
				       struct comp_ipc_config *ipc_config,
				       const void *ipc_specific_config)
{
	struct sof_man_fw_desc *desc;
	struct sof_man_module *mod;
	const struct ipc4_base_module_cfg *base_cfg = ipc_specific_config;
	int ret;
	uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);

	tr_dbg(&lib_manager_tr, "llext_manager_allocate_module(): mod_id: %#x",
	       ipc_config->id);

	desc = lib_manager_get_library_module_desc(module_id);
	if (!ctx || !desc) {
		tr_err(&lib_manager_tr,
		       "llext_manager_allocate_module(): failed to get module descriptor");
		return 0;
	}

	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(entry_index));

	for (unsigned int i = 0; i < ARRAY_SIZE(ctx->segment_size); i++)
		ctx->segment_size[i] = mod->segment[i].flags.r.length * PAGE_SZ;

	ret = llext_manager_load_module(module_id, mod, desc);
	if (ret < 0)
		return 0;

	ret = llext_manager_allocate_module_bss(module_id, base_cfg->is_pages, mod);
	if (ret < 0) {
		tr_err(&lib_manager_tr,
		       "llext_manager_allocate_module(): module allocation failed: %d", ret);
		return 0;
	}
	return mod->entry_point;
}

int llext_manager_free_module(const struct comp_driver *drv,
			      struct comp_ipc_config *ipc_config)
{
	struct sof_man_fw_desc *desc;
	struct sof_man_module *mod;
	uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);
	int ret;

	tr_dbg(&lib_manager_tr, "llext_manager_free_module(): mod_id: %#x", ipc_config->id);

	desc = lib_manager_get_library_module_desc(module_id);
	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(entry_index));

	ret = llext_manager_unload_module(module_id, mod, desc);
	if (ret < 0)
		return ret;

	ret = llext_manager_free_module_bss(module_id, mod);
	if (ret < 0) {
		tr_err(&lib_manager_tr,
		       "llext_manager_free_module(): free module bss failed: %d", ret);
		return ret;
	}
	return 0;
}

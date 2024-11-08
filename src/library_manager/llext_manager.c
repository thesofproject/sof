// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
//         Pawel Dobrowolski<pawelx.dobrowolski@intel.com>

/*
 * Dynamic module loading functions using Zephyr Linkable Loadable Extensions (LLEXT) interface.
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
#include <zephyr/llext/buf_loader.h>
#include <zephyr/llext/loader.h>
#include <zephyr/llext/llext.h>

#include <rimage/sof/user/manifest.h>
#include <module/module/api_ver.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * FIXME: this definition is copied from tools/rimage/src/include/rimage/manifest.h
 * which we cannot easily include here, because it also pulls in
 * tools/rimage/src/include/rimage/elf.h which then conflicts with
 * zephyr/include/zephyr/llext/elf.h
 */
#define FILE_TEXT_OFFSET_V1_8		0x8000

LOG_MODULE_DECLARE(lib_manager, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx lib_manager_tr;

#define PAGE_SZ		CONFIG_MM_DRV_PAGE_SIZE

static int llext_manager_update_flags(void __sparse_cache *vma, size_t size, uint32_t flags)
{
	size_t pre_pad_size = (uintptr_t)vma & (PAGE_SZ - 1);
	void *aligned_vma = (__sparse_force uint8_t *)vma - pre_pad_size;

	return sys_mm_drv_update_region_flags(aligned_vma,
					      ALIGN_UP(pre_pad_size + size, PAGE_SZ), flags);
}

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
	int ret = llext_manager_align_map(vma, size, SYS_MM_MEM_PERM_RW);

	if (ret < 0) {
		tr_err(&lib_manager_tr, "cannot map %u of %p", size, (__sparse_force void *)vma);
		return ret;
	}

	ret = memcpy_s((__sparse_force void *)vma, size, s_addr, size);
	if (ret < 0)
		return ret;

	/*
	 * We don't know what flags we're changing to, maybe the buffer will be
	 * executable or read-only. Need to write back caches now
	 */
	dcache_writeback_region(vma, size);

	ret = llext_manager_update_flags(vma, size, flags);
	if (!ret && (flags & SYS_MM_MEM_PERM_EXEC))
		icache_invalidate_region(vma, size);

	return ret;
}

static int llext_manager_load_module(uint32_t module_id, const struct sof_man_module *mod)
{
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	uint8_t *load_base = (uint8_t *)ctx->base_addr;

	/* Executable code (.text) */
	void __sparse_cache *va_base_text = (void __sparse_cache *)
		ctx->segment[LIB_MANAGER_TEXT].addr;
	void *src_txt = (void *)(load_base + ctx->segment[LIB_MANAGER_TEXT].file_offset);
	size_t text_size = ctx->segment[LIB_MANAGER_TEXT].size;

	/* Read-only data (.rodata and others) */
	void __sparse_cache *va_base_rodata = (void __sparse_cache *)
		ctx->segment[LIB_MANAGER_RODATA].addr;
	void *src_rodata = (void *)(load_base +
				    ctx->segment[LIB_MANAGER_RODATA].file_offset);
	size_t rodata_size = ctx->segment[LIB_MANAGER_RODATA].size;

	/* Writable data (.data, .bss and others) */
	void __sparse_cache *va_base_data = (void __sparse_cache *)
		ctx->segment[LIB_MANAGER_DATA].addr;
	void *src_data = (void *)(load_base +
				  ctx->segment[LIB_MANAGER_DATA].file_offset);
	size_t data_size = ctx->segment[LIB_MANAGER_DATA].size;

	/* .bss, should be within writable data above */
	void __sparse_cache *bss_addr = (void __sparse_cache *)
		ctx->segment[LIB_MANAGER_BSS].addr;
	size_t bss_size = ctx->segment[LIB_MANAGER_BSS].size;
	int ret;

	/* Check, that .bss is within .data */
	if (bss_size &&
	    ((uintptr_t)bss_addr + bss_size <= (uintptr_t)va_base_data ||
	     (uintptr_t)bss_addr >= (uintptr_t)va_base_data + data_size)) {
		if ((uintptr_t)bss_addr + bss_size == (uintptr_t)va_base_data &&
		    !((uintptr_t)bss_addr & (PAGE_SZ - 1))) {
			/* .bss directly in front of writable data and properly aligned, prepend */
			va_base_data = bss_addr;
			data_size += bss_size;
		} else if ((uintptr_t)bss_addr == (uintptr_t)va_base_data + data_size) {
			/* .bss directly behind writable data, append */
			data_size += bss_size;
		} else {
			tr_err(&lib_manager_tr, ".bss %#x @%p isn't within writable data %#x @%p!",
			       bss_size, (__sparse_force void *)bss_addr,
			       data_size, (__sparse_force void *)va_base_data);
			return -EPROTO;
		}
	}

	/* Copy Code */
	ret = llext_manager_load_data_from_storage(va_base_text, src_txt, text_size,
						   SYS_MM_MEM_PERM_EXEC);
	if (ret < 0)
		return ret;

	/* Copy read-only data */
	ret = llext_manager_load_data_from_storage(va_base_rodata, src_rodata,
						   rodata_size, 0);
	if (ret < 0)
		goto e_text;

	/* Copy writable data */
	ret = llext_manager_load_data_from_storage(va_base_data, src_data,
						   data_size, SYS_MM_MEM_PERM_RW);
	if (ret < 0)
		goto e_rodata;

	memset((__sparse_force void *)bss_addr, 0, bss_size);

	return 0;

e_rodata:
	llext_manager_align_unmap(va_base_rodata, rodata_size);
e_text:
	llext_manager_align_unmap(va_base_text, text_size);

	return ret;
}

static int llext_manager_unload_module(uint32_t module_id, const struct sof_man_module *mod)
{
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	/* Executable code (.text) */
	void __sparse_cache *va_base_text = (void __sparse_cache *)
		ctx->segment[LIB_MANAGER_TEXT].addr;
	size_t text_size = ctx->segment[LIB_MANAGER_TEXT].size;

	/* Read-only data (.rodata, etc.) */
	void __sparse_cache *va_base_rodata = (void __sparse_cache *)
		ctx->segment[LIB_MANAGER_RODATA].addr;
	size_t rodata_size = ctx->segment[LIB_MANAGER_RODATA].size;

	/* Writable data (.data, .bss, etc.) */
	void __sparse_cache *va_base_data = (void __sparse_cache *)
		ctx->segment[LIB_MANAGER_DATA].addr;
	size_t data_size = ctx->segment[LIB_MANAGER_DATA].size;
	int err = 0, ret;

	ret = llext_manager_align_unmap(va_base_text, text_size);
	if (ret < 0)
		err = ret;

	ret = llext_manager_align_unmap(va_base_data, data_size);
	if (ret < 0 && !err)
		err = ret;

	ret = llext_manager_align_unmap(va_base_rodata, rodata_size);
	if (ret < 0 && !err)
		err = ret;

	return err;
}

static int llext_manager_link(struct sof_man_fw_desc *desc, struct sof_man_module *mod,
			      uint32_t module_id, struct module_data *md, const void **buildinfo,
			      const struct sof_man_module_manifest **mod_manifest)
{
	size_t mod_size = desc->header.preload_page_count * PAGE_SZ - FILE_TEXT_OFFSET_V1_8;
	uintptr_t dram_base = (uintptr_t)desc - SOF_MAN_ELF_TEXT_OFFSET;
	struct llext_buf_loader ebl = LLEXT_BUF_LOADER((uint8_t *)dram_base + FILE_TEXT_OFFSET_V1_8,
						       mod_size);
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	/* Identify if this is the first time loading this module */
	struct llext_load_param ldr_parm = {
		.relocate_local = !ctx->segment[LIB_MANAGER_TEXT].size,
		.pre_located = true,
	};
	int ret = llext_load(&ebl.loader, mod->name, &md->llext, &ldr_parm);

	if (ret)
		return ret;

	ctx->segment[LIB_MANAGER_TEXT].addr = ebl.loader.sects[LLEXT_MEM_TEXT].sh_addr;
	ctx->segment[LIB_MANAGER_TEXT].file_offset =
		(uintptr_t)md->llext->mem[LLEXT_MEM_TEXT] - dram_base;
	ctx->segment[LIB_MANAGER_TEXT].size = ebl.loader.sects[LLEXT_MEM_TEXT].sh_size;

	tr_dbg(&lib_manager_tr, ".text: start: %#lx size %#x offset %#x",
	       ctx->segment[LIB_MANAGER_TEXT].addr,
	       ctx->segment[LIB_MANAGER_TEXT].size,
	       ctx->segment[LIB_MANAGER_TEXT].file_offset);

	/* This contains all other sections, except .text, it might contain .bss too */
	ctx->segment[LIB_MANAGER_RODATA].addr =
		ebl.loader.sects[LLEXT_MEM_RODATA].sh_addr;
	ctx->segment[LIB_MANAGER_RODATA].file_offset =
		(uintptr_t)md->llext->mem[LLEXT_MEM_RODATA] - dram_base;
	ctx->segment[LIB_MANAGER_RODATA].size = ebl.loader.sects[LLEXT_MEM_RODATA].sh_size;

	tr_dbg(&lib_manager_tr, ".rodata: start: %#lx size %#x offset %#x",
	       ctx->segment[LIB_MANAGER_RODATA].addr,
	       ctx->segment[LIB_MANAGER_RODATA].size,
	       ctx->segment[LIB_MANAGER_RODATA].file_offset);

	ctx->segment[LIB_MANAGER_DATA].addr =
		ebl.loader.sects[LLEXT_MEM_DATA].sh_addr;
	ctx->segment[LIB_MANAGER_DATA].file_offset =
		(uintptr_t)md->llext->mem[LLEXT_MEM_DATA] - dram_base;
	ctx->segment[LIB_MANAGER_DATA].size = ebl.loader.sects[LLEXT_MEM_DATA].sh_size;

	tr_dbg(&lib_manager_tr, ".data: start: %#lx size %#x offset %#x",
	       ctx->segment[LIB_MANAGER_DATA].addr,
	       ctx->segment[LIB_MANAGER_DATA].size,
	       ctx->segment[LIB_MANAGER_DATA].file_offset);

	ctx->segment[LIB_MANAGER_BSS].addr = ebl.loader.sects[LLEXT_MEM_BSS].sh_addr;
	ctx->segment[LIB_MANAGER_BSS].size = ebl.loader.sects[LLEXT_MEM_BSS].sh_size;

	tr_dbg(&lib_manager_tr, ".bss: start: %#lx size %#x",
	       ctx->segment[LIB_MANAGER_BSS].addr,
	       ctx->segment[LIB_MANAGER_BSS].size);

	ssize_t binfo_o = llext_find_section(&ebl.loader, ".mod_buildinfo");

	if (binfo_o >= 0)
		*buildinfo = llext_peek(&ebl.loader, binfo_o);

	ssize_t mod_o = llext_find_section(&ebl.loader, ".module");

	if (mod_o >= 0)
		*mod_manifest = llext_peek(&ebl.loader, mod_o);

	return binfo_o >= 0 && mod_o >= 0 ? 0 : -EPROTO;
}

uintptr_t llext_manager_allocate_module(struct processing_module *proc,
					const struct comp_ipc_config *ipc_config,
					const void *ipc_specific_config)
{
	struct sof_man_fw_desc *desc;
	struct sof_man_module *mod_array;
	int ret;
	uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	const struct sof_module_api_build_info *buildinfo;
	const struct sof_man_module_manifest *mod_manifest;

	tr_dbg(&lib_manager_tr, "llext_manager_allocate_module(): mod_id: %#x",
	       ipc_config->id);

	desc = (struct sof_man_fw_desc *)lib_manager_get_library_manifest(module_id);
	if (!ctx || !desc) {
		tr_err(&lib_manager_tr,
		       "llext_manager_allocate_module(): failed to get module descriptor");
		return 0;
	}

	mod_array = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(0));

	/* LLEXT linking is only needed once for all the modules in the library */
	ret = llext_manager_link(desc, mod_array, module_id, &proc->priv, (const void **)&buildinfo,
				 &mod_manifest);
	if (ret < 0)
		return 0;

	if (!ret) {
		/* First instance: check that the module is native */
		if (buildinfo->format != SOF_MODULE_API_BUILD_INFO_FORMAT ||
		    buildinfo->api_version_number.full != SOF_MODULE_API_CURRENT_VERSION) {
			tr_err(&lib_manager_tr,
			       "llext_manager_allocate_module(): Unsupported module API version");
			return -ENOEXEC;
		}

		/* Map executable code and data */
		ret = llext_manager_load_module(module_id, mod_array);
		if (ret < 0)
			return 0;

		/* Manifest is in read-only data */
		uintptr_t dram_rodata = (uintptr_t)ctx->base_addr +
			ctx->segment[LIB_MANAGER_RODATA].file_offset;
		uintptr_t va_rodata_base = ctx->segment[LIB_MANAGER_RODATA].addr;
		size_t offset = (uintptr_t)mod_manifest - dram_rodata;

		/* ctx->mod_manifest points to an array of module manifests */
		ctx->mod_manifest = sys_cache_uncached_ptr_get((__sparse_force void __sparse_cache *)
							       (va_rodata_base + offset));
	}

	return ctx->mod_manifest[entry_index].module.entry_point;
}

int llext_manager_free_module(const uint32_t component_id)
{
	const struct sof_man_module *mod;
	const uint32_t module_id = IPC4_MOD_ID(component_id);
	const unsigned int base_module_id = LIB_MANAGER_GET_LIB_ID(module_id) <<
		LIB_MANAGER_LIB_ID_SHIFT;

	tr_dbg(&lib_manager_tr, "llext_manager_free_module(): mod_id: %#x", component_id);

	mod = lib_manager_get_module_manifest(base_module_id);

	return llext_manager_unload_module(base_module_id, mod);
}

bool comp_is_llext(struct comp_dev *comp)
{
	const uint32_t module_id = IPC4_MOD_ID(comp->ipc_config.id);
	const unsigned int base_module_id = LIB_MANAGER_GET_LIB_ID(module_id) <<
		LIB_MANAGER_LIB_ID_SHIFT;
	const struct sof_man_module *mod = lib_manager_get_module_manifest(base_module_id);

	return mod && module_is_llext(mod);
}

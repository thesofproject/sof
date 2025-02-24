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
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/llext/inspect.h>

#include <rimage/sof/user/manifest.h>
#include <module/module/api_ver.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/*
 * Map the memory range covered by 'vma' and 'size' as writable, copy all
 * sections that belong to the specified 'region' and are contained in the
 * memory range, then remap the same area according to the 'flags' parameter.
 */
static int llext_manager_load_data_from_storage(const struct llext_loader *ldr,
						const struct llext *ext,
						enum llext_mem region,
						void __sparse_cache *vma,
						size_t size, uint32_t flags)
{
	unsigned int i;
	const void *region_addr;
	int ret = llext_manager_align_map(vma, size, SYS_MM_MEM_PERM_RW);

	if (ret < 0) {
		tr_err(&lib_manager_tr, "cannot map %u of %p", size, (__sparse_force void *)vma);
		return ret;
	}

	llext_get_region_info(ldr, ext, region, NULL, &region_addr, NULL);

	/* Need to copy sections within regions individually, offsets may differ */
	for (i = 0; i < llext_section_count(ext); i++) {
		const elf_shdr_t *shdr;
		enum llext_mem s_region = LLEXT_MEM_COUNT;
		size_t s_offset = 0;

		llext_get_section_info(ldr, ext, i, &shdr, &s_region, &s_offset);

		/* skip sections not in the requested region */
		if (s_region != region)
			continue;

		/* skip detached sections (will be outside requested VMA area) */
		if ((uintptr_t)shdr->sh_addr < (uintptr_t)vma ||
		    (uintptr_t)shdr->sh_addr >= (uintptr_t)vma + size)
			continue;

		ret = memcpy_s((__sparse_force void *)shdr->sh_addr, size - s_offset,
			       (const uint8_t *)region_addr + s_offset, shdr->sh_size);
		if (ret < 0)
			return ret;
	}

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

static int llext_manager_load_module(struct lib_manager_module *mctx)
{
	/* Executable code (.text) */
	void __sparse_cache *va_base_text = (void __sparse_cache *)
		mctx->segment[LIB_MANAGER_TEXT].addr;
	size_t text_size = mctx->segment[LIB_MANAGER_TEXT].size;

	/* Read-only data (.rodata and others) */
	void __sparse_cache *va_base_rodata = (void __sparse_cache *)
		mctx->segment[LIB_MANAGER_RODATA].addr;
	size_t rodata_size = mctx->segment[LIB_MANAGER_RODATA].size;

	/* Writable data (.data, .bss and others) */
	void __sparse_cache *va_base_data = (void __sparse_cache *)
		mctx->segment[LIB_MANAGER_DATA].addr;
	size_t data_size = mctx->segment[LIB_MANAGER_DATA].size;

	/* .bss, should be within writable data above */
	void __sparse_cache *bss_addr = (void __sparse_cache *)
		mctx->segment[LIB_MANAGER_BSS].addr;
	size_t bss_size = mctx->segment[LIB_MANAGER_BSS].size;
	int ret;

	/* Check, that .bss is within .data */
	if (bss_size &&
	    ((uintptr_t)bss_addr + bss_size <= (uintptr_t)va_base_data ||
	     (uintptr_t)bss_addr >= (uintptr_t)va_base_data + data_size)) {
		size_t bss_align = MIN(PAGE_SZ, BIT(__builtin_ctz((uintptr_t)bss_addr)));

		if ((uintptr_t)bss_addr + bss_size == (uintptr_t)va_base_data &&
		    !((uintptr_t)bss_addr & (PAGE_SZ - 1))) {
			/* .bss directly in front of writable data and properly aligned, prepend */
			va_base_data = bss_addr;
			data_size += bss_size;
		} else if ((uintptr_t)bss_addr == (uintptr_t)va_base_data +
			   ALIGN_UP(data_size, bss_align)) {
			/* .bss directly behind writable data, append */
			data_size += bss_size;
		} else {
			tr_err(&lib_manager_tr, ".bss %#x @%p isn't within writable data %#x @%p!",
			       bss_size, (__sparse_force void *)bss_addr,
			       data_size, (__sparse_force void *)va_base_data);
			return -EPROTO;
		}
	}

	const struct llext_loader *ldr = &mctx->ebl->loader;
	const struct llext *ext = mctx->llext;

	/* Copy Code */
	ret = llext_manager_load_data_from_storage(ldr, ext, LLEXT_MEM_TEXT,
						   va_base_text, text_size, SYS_MM_MEM_PERM_EXEC);
	if (ret < 0)
		return ret;

	/* Copy read-only data */
	ret = llext_manager_load_data_from_storage(ldr, ext, LLEXT_MEM_RODATA,
						   va_base_rodata, rodata_size, 0);
	if (ret < 0)
		goto e_text;

	/* Copy writable data */
	/*
	 * NOTE: va_base_data and data_size refer to an address range that
	 *       spans over the BSS area as well, so the mapping will cover
	 *       both, but only LLEXT_MEM_DATA sections will be copied.
	 */
	ret = llext_manager_load_data_from_storage(ldr, ext, LLEXT_MEM_DATA,
						   va_base_data, data_size, SYS_MM_MEM_PERM_RW);
	if (ret < 0)
		goto e_rodata;

	memset((__sparse_force void *)bss_addr, 0, bss_size);
	mctx->mapped = true;

	return 0;

e_rodata:
	llext_manager_align_unmap(va_base_rodata, rodata_size);
e_text:
	llext_manager_align_unmap(va_base_text, text_size);

	return ret;
}

static int llext_manager_unload_module(struct lib_manager_module *mctx)
{
	/* Executable code (.text) */
	void __sparse_cache *va_base_text = (void __sparse_cache *)
		mctx->segment[LIB_MANAGER_TEXT].addr;
	size_t text_size = mctx->segment[LIB_MANAGER_TEXT].size;

	/* Read-only data (.rodata, etc.) */
	void __sparse_cache *va_base_rodata = (void __sparse_cache *)
		mctx->segment[LIB_MANAGER_RODATA].addr;
	size_t rodata_size = mctx->segment[LIB_MANAGER_RODATA].size;

	/* Writable data (.data, .bss, etc.) */
	void __sparse_cache *va_base_data = (void __sparse_cache *)
		mctx->segment[LIB_MANAGER_DATA].addr;
	size_t data_size = mctx->segment[LIB_MANAGER_DATA].size;
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

	mctx->mapped = false;

	return err;
}

static bool llext_manager_section_detached(const elf_shdr_t *shdr)
{
	return shdr->sh_addr < SOF_MODULE_DRAM_LINK_END;
}

static int llext_manager_link(const char *name,
			      struct lib_manager_module *mctx, const void **buildinfo,
			      const struct sof_man_module_manifest **mod_manifest)
{
	struct llext **llext = &mctx->llext;
	struct llext_loader *ldr = &mctx->ebl->loader;
	const elf_shdr_t *hdr;
	int ret;

	if (*llext && !mctx->mapped) {
		/*
		 * All module instances have been terminated, so we freed SRAM,
		 * but we kept the full Zephyr LLEXT context. Now a new instance
		 * is starting, so we just re-use all the configuration and only
		 * re-allocate SRAM and copy the module into it
		 */
		*mod_manifest = mctx->mod_manifest;

		return 0;
	}

	if (!*llext || mctx->mapped) {
		/*
		 * Either the very first time loading this module, or the module
		 * is already mapped, we just call llext_load() to refcount it
		 */
		struct llext_load_param ldr_parm = {
			.relocate_local = !*llext,
			.pre_located = true,
			.section_detached = llext_manager_section_detached,
			.keep_section_info = true,
		};

		ret = llext_load(ldr, name, llext, &ldr_parm);
		if (ret)
			return ret;
	}

	/* All code sections */
	llext_get_region_info(ldr, *llext, LLEXT_MEM_TEXT, &hdr, NULL, NULL);
	mctx->segment[LIB_MANAGER_TEXT].addr = hdr->sh_addr;
	mctx->segment[LIB_MANAGER_TEXT].size = hdr->sh_size;

	tr_dbg(&lib_manager_tr, ".text: start: %#lx size %#x",
	       mctx->segment[LIB_MANAGER_TEXT].addr,
	       mctx->segment[LIB_MANAGER_TEXT].size);

	/* All read-only data sections */
	llext_get_region_info(ldr, *llext, LLEXT_MEM_RODATA, &hdr, NULL, NULL);
	mctx->segment[LIB_MANAGER_RODATA].addr = hdr->sh_addr;
	mctx->segment[LIB_MANAGER_RODATA].size = hdr->sh_size;

	tr_dbg(&lib_manager_tr, ".rodata: start: %#lx size %#x",
	       mctx->segment[LIB_MANAGER_RODATA].addr,
	       mctx->segment[LIB_MANAGER_RODATA].size);

	/* All writable data sections */
	llext_get_region_info(ldr, *llext, LLEXT_MEM_DATA, &hdr, NULL, NULL);
	mctx->segment[LIB_MANAGER_DATA].addr = hdr->sh_addr;
	mctx->segment[LIB_MANAGER_DATA].size = hdr->sh_size;

	tr_dbg(&lib_manager_tr, ".data: start: %#lx size %#x",
	       mctx->segment[LIB_MANAGER_DATA].addr,
	       mctx->segment[LIB_MANAGER_DATA].size);

	/* Writable uninitialized data section */
	llext_get_region_info(ldr, *llext, LLEXT_MEM_BSS, &hdr, NULL, NULL);
	mctx->segment[LIB_MANAGER_BSS].addr = hdr->sh_addr;
	mctx->segment[LIB_MANAGER_BSS].size = hdr->sh_size;

	tr_dbg(&lib_manager_tr, ".bss: start: %#lx size %#x",
	       mctx->segment[LIB_MANAGER_BSS].addr,
	       mctx->segment[LIB_MANAGER_BSS].size);

	*buildinfo = NULL;
	ret = llext_section_shndx(ldr, *llext, ".mod_buildinfo");
	if (ret >= 0) {
		llext_get_section_info(ldr, *llext, ret, &hdr, NULL, NULL);
		*buildinfo = llext_peek(ldr, hdr->sh_offset);
	}

	*mod_manifest = NULL;
	ret = llext_section_shndx(ldr, *llext, ".module");
	if (ret >= 0) {
		llext_get_section_info(ldr, *llext, ret, &hdr, NULL, NULL);
		*mod_manifest = llext_peek(ldr, hdr->sh_offset);
	}

	return *buildinfo && *mod_manifest ? 0 : -EPROTO;
}

/* Count "module files" in the library, allocate and initialize memory for their descriptors */
static int llext_manager_mod_init(struct lib_manager_mod_ctx *ctx,
				  const struct sof_man_fw_desc *desc)
{
	struct sof_man_module *mod_array = (struct sof_man_module *)((uint8_t *)desc +
								     SOF_MAN_MODULE_OFFSET(0));
	unsigned int i, n_mod;
	size_t offs;

	/* count modules */
	for (i = 0, n_mod = 0, offs = ~0; i < desc->header.num_module_entries; i++)
		if (mod_array[i].segment[LIB_MANAGER_TEXT].file_offset != offs) {
			offs = mod_array[i].segment[LIB_MANAGER_TEXT].file_offset;
			n_mod++;
		}

	/*
	 * Loadable modules are loaded to DRAM once and never unloaded from it.
	 * Context, related to them, is never freed
	 */
	ctx->mod = rmalloc(SOF_MEM_ZONE_RUNTIME_SHARED, SOF_MEM_FLAG_COHERENT,
			   SOF_MEM_CAPS_RAM, n_mod * sizeof(ctx->mod[0]));
	if (!ctx->mod)
		return -ENOMEM;

	ctx->n_mod = n_mod;

	for (i = 0, n_mod = 0, offs = ~0; i < desc->header.num_module_entries; i++)
		if (mod_array[i].segment[LIB_MANAGER_TEXT].file_offset != offs) {
			offs = mod_array[i].segment[LIB_MANAGER_TEXT].file_offset;
			ctx->mod[n_mod].mapped = false;
			ctx->mod[n_mod].llext = NULL;
			ctx->mod[n_mod].ebl = NULL;
			ctx->mod[n_mod++].start_idx = i;
		}

	return 0;
}

/* Find a module context, containing the driver with the supplied index */
static unsigned int llext_manager_mod_find(const struct lib_manager_mod_ctx *ctx, unsigned int idx)
{
	unsigned int i;

	for (i = 0; i < ctx->n_mod; i++)
		if (ctx->mod[i].start_idx > idx)
			break;

	return i - 1;
}

static int llext_manager_link_single(uint32_t module_id, const struct sof_man_fw_desc *desc,
				     struct lib_manager_mod_ctx *ctx, const void **buildinfo,
				     const struct sof_man_module_manifest **mod_manifest)
{
	struct sof_man_module *mod_array = (struct sof_man_module *)((uint8_t *)desc +
								     SOF_MAN_MODULE_OFFSET(0));
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);
	size_t mod_offset = mod_array[entry_index].segment[LIB_MANAGER_TEXT].file_offset;
	int ret;

	tr_dbg(&lib_manager_tr, "mod_id: %u", module_id);

	if (entry_index >= desc->header.num_module_entries) {
		tr_err(&lib_manager_tr, "Invalid driver index %u exceeds %d",
		       entry_index, desc->header.num_module_entries - 1);
		return -EINVAL;
	}

	unsigned int mod_ctx_idx = llext_manager_mod_find(ctx, entry_index);
	struct lib_manager_module *mctx = ctx->mod + mod_ctx_idx;
	size_t mod_size;
	int i, inst_idx;

	/*
	 * We don't know the number of ELF files that this library is built of.
	 * We know the number of module drivers, but each of those ELF files can
	 * also contain multiple such drivers. Each driver brings two copies of
	 * its manifest with it: one in the ".module" ELF section and one in an
	 * array of manifests at the beginning of the library. This latter array
	 * is created from a TOML configuration file. The order is preserved -
	 * this is guaranteed by rimage.
	 * All module drivers within a single ELF file have equal .file_offset,
	 * this makes it possible to find borders between them.
	 * We know the global index of the requested driver in that array, but
	 * we need to find the matching manifest in ".module" because only it
	 * contains the entry point. For safety we calculate the ELF driver
	 * index and then also check the driver name.
	 * We also need a module size. For this we search the manifest array for
	 * the next ELF file, then the difference between offsets gives us the
	 * module size.
	 */
	for (i = entry_index - 1; i >= 0; i--)
		if (mod_array[i].segment[LIB_MANAGER_TEXT].file_offset != mod_offset)
			break;

	/* Driver index within a single module */
	inst_idx = entry_index - i - 1;

	/* Find the next module or stop at the end */
	for (i = entry_index + 1; i < desc->header.num_module_entries; i++)
		if (mod_array[i].segment[LIB_MANAGER_TEXT].file_offset != mod_offset)
			break;

	if (i == desc->header.num_module_entries)
		mod_size = desc->header.preload_page_count * PAGE_SZ - mod_offset;
	else
		mod_size = ALIGN_UP(mod_array[i].segment[LIB_MANAGER_TEXT].file_offset - mod_offset,
				    PAGE_SZ);

	if (!mctx->ebl) {
		/* allocate once, never freed */
		mctx->ebl = rmalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
				    sizeof(struct llext_buf_loader));
		if (!mctx->ebl) {
			tr_err(&lib_manager_tr, "loader alloc failed");
			return 0;
		}

		uint8_t *dram_base = (uint8_t *)desc - SOF_MAN_ELF_TEXT_OFFSET;

		*mctx->ebl = (struct llext_buf_loader)LLEXT_BUF_LOADER(dram_base + mod_offset,
								       mod_size);
	}

	/*
	 * LLEXT linking is only needed once for all the "drivers" in the
	 * module. This calls llext_load(), which also takes references to any
	 * dependencies, sets up sections and retrieves buildinfo and
	 * mod_manifest
	 */
	ret = llext_manager_link(mod_array[entry_index - inst_idx].name, mctx,
				 buildinfo, mod_manifest);
	if (ret < 0) {
		tr_err(&lib_manager_tr, "linking failed: %d", ret);
		return ret;
	}

	/* if ret > 0, then the "driver" is already loaded */
	if (!ret)
		/* mctx->mod_manifest points to a const array of module manifests */
		mctx->mod_manifest = *mod_manifest;

	/* Return the manifest, related to the specific instance */
	*mod_manifest = mctx->mod_manifest + inst_idx;

	if (strncmp(mod_array[entry_index].name, (*mod_manifest)->module.name,
		    sizeof(mod_array[0].name))) {
		tr_err(&lib_manager_tr, "Name mismatch %s vs. %s",
		       mod_array[entry_index].name, (*mod_manifest)->module.name);
		return -ENOEXEC;
	}

	return mod_ctx_idx;
}

static int llext_lib_find(const struct llext *llext, struct lib_manager_module **dep_ctx)
{
	struct ext_library *_ext_lib = ext_lib_get();
	unsigned int i, j;

	if (!llext)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(_ext_lib->desc); i++) {
		if (!_ext_lib->desc[i])
			continue;

		for (j = 0; j < _ext_lib->desc[i]->n_mod; j++)
			if (_ext_lib->desc[i]->mod[j].llext == llext) {
				*dep_ctx = _ext_lib->desc[i]->mod + j;
				return i;
			}
	}

	return -ENOENT;
}

static void llext_manager_depend_unlink_rollback(struct lib_manager_module *dep_ctx[], int n)
{
	for (; n >= 0; n--)
		if (dep_ctx[n] && dep_ctx[n]->llext->use_count == 1)
			llext_manager_unload_module(dep_ctx[n]);
}

uintptr_t llext_manager_allocate_module(const struct comp_ipc_config *ipc_config,
					const void *ipc_specific_config)
{
	uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	/* Library manifest */
	const struct sof_man_fw_desc *desc = (struct sof_man_fw_desc *)
		lib_manager_get_library_manifest(module_id);
	/* Library context */
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);

	if (!ctx || !desc) {
		tr_err(&lib_manager_tr, "failed to get module descriptor");
		return 0;
	}

	/* Array of all "module drivers" (manifests) in the library */
	const struct sof_man_module_manifest *mod_manifest;
	const struct sof_module_api_build_info *buildinfo = NULL;

	/* "module file" index in the ctx->mod array */
	int mod_ctx_idx = llext_manager_link_single(module_id, desc, ctx,
						    (const void **)&buildinfo, &mod_manifest);

	if (mod_ctx_idx < 0)
		return 0;

	struct lib_manager_module *mctx = ctx->mod + mod_ctx_idx;

	if (buildinfo) {
		/* First instance: check that the module is native */
		if (buildinfo->format != SOF_MODULE_API_BUILD_INFO_FORMAT ||
		    buildinfo->api_version_number.full != SOF_MODULE_API_CURRENT_VERSION) {
			tr_err(&lib_manager_tr, "Unsupported module API version");
			return 0;
		}
	}

	if (!mctx->mapped) {
		int i, ret;

		/*
		 * Check if any dependencies need to be mapped - collect
		 * pointers to library contexts
		 */
		struct lib_manager_module *dep_ctx[LLEXT_MAX_DEPENDENCIES] = {};

		for (i = 0; i < ARRAY_SIZE(mctx->llext->dependency); i++) {
			/* Dependencies are filled from the beginning of the array upwards */
			if (!mctx->llext->dependency[i])
				break;

			/*
			 * Protected by the IPC serialization, but maybe we should protect the
			 * use-count explicitly too. Currently the use-count is first incremented
			 * when an auxiliary library is loaded, it was then additionally incremented
			 * when the current dependent module was mapped. If it's higher than two,
			 * then some other modules also depend on it and have already mapped it.
			 */
			if (mctx->llext->dependency[i]->use_count > 2)
				continue;

			/* First user of this dependency, load it into SRAM */
			ret = llext_lib_find(mctx->llext->dependency[i], &dep_ctx[i]);
			if (ret < 0) {
				tr_err(&lib_manager_tr,
				       "Unmet dependency: cannot find dependency %u", i);
				continue;
			}

			tr_dbg(&lib_manager_tr, "%s depending on %s index %u, %u users",
			       mctx->llext->name, mctx->llext->dependency[i]->name,
			       dep_ctx[i]->start_idx, mctx->llext->dependency[i]->use_count);

			ret = llext_manager_load_module(dep_ctx[i]);
			if (ret < 0) {
				llext_manager_depend_unlink_rollback(dep_ctx, i - 1);
				return 0;
			}
		}

		/* Map executable code and data */
		ret = llext_manager_load_module(mctx);
		if (ret < 0)
			return 0;
	}

	return mod_manifest->module.entry_point;
}

int llext_manager_free_module(const uint32_t component_id)
{
	const uint32_t module_id = IPC4_MOD_ID(component_id);
	struct sof_man_fw_desc *desc = (struct sof_man_fw_desc *)lib_manager_get_library_manifest(module_id);
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);

	if (entry_index >= desc->header.num_module_entries) {
		tr_err(&lib_manager_tr, "Invalid driver index %u exceeds %d",
		       entry_index, desc->header.num_module_entries - 1);
		return -ENOENT;
	}

	if (!ctx->mod) {
		tr_err(&lib_manager_tr, "NULL module array: ID %#x ctx %p", component_id, ctx);
		return -ENOENT;
	}

	unsigned int mod_idx = llext_manager_mod_find(ctx, entry_index);
	struct lib_manager_module *mctx = ctx->mod + mod_idx;

	/* Protected by IPC serialization */
	if (mctx->llext->use_count > 1) {
		/*
		 * At least 2 users: llext_unload() will never actually free
		 * the extension but only reduce the refcount and return its
		 * new value (must be a positive number).
		 * NOTE: if this is modified to allow extension unload, the
		 * inspection data in the loader must be freed as well by
		 * calling the llext_free_inspection_data() function.
		 */
		int ret = llext_unload(&mctx->llext);

		if (ret <= 0) {
			tr_err(&lib_manager_tr,
			       "mod_id: %#x: invalid return code from llext_unload(): %d",
			       component_id, ret);
			return ret ? : -EPROTO;
		}

		/* More users are active */
		return 0;
	}

	struct lib_manager_module *dep_ctx[LLEXT_MAX_DEPENDENCIES] = {};
	int i;	/* signed to match llext_manager_depend_unlink_rollback() */

	for (i = 0; i < ARRAY_SIZE(mctx->llext->dependency); i++)
		if (llext_lib_find(mctx->llext->dependency[i], &dep_ctx[i]) < 0)
			break;

	/* Last user cleaning up, put dependencies */
	if (i)
		llext_manager_depend_unlink_rollback(dep_ctx, i - 1);

	/*
	 * The last instance of the module has been destroyed and it can now be
	 * unloaded from SRAM
	 */
	tr_dbg(&lib_manager_tr, "mod_id: %#x", component_id);

	/* Since the LLEXT context now is preserved, we have to flush logs ourselves */
	log_flush();

	return llext_manager_unload_module(mctx);
}

/* An auxiliary library has been loaded, need to read in its exported symbols */
int llext_manager_add_library(uint32_t module_id)
{
	struct lib_manager_mod_ctx *const ctx = lib_manager_get_mod_ctx(module_id);

	if (ctx->mod) {
		tr_err(&lib_manager_tr, "module_id: %#x: repeated load!", module_id);
		return -EBUSY;
	}

	const struct sof_man_fw_desc *desc = lib_manager_get_library_manifest(module_id);
	unsigned int i;

	if (!ctx->mod)
		llext_manager_mod_init(ctx, desc);

	for (i = 0; i < ctx->n_mod; i++) {
		const struct sof_man_module *mod = lib_manager_get_module_manifest(module_id + i);

		if (mod->type.load_type == SOF_MAN_MOD_TYPE_LLEXT_AUX) {
			const struct sof_man_module_manifest *mod_manifest;
			const struct sof_module_api_build_info *buildinfo;
			int ret = llext_manager_link_single(module_id + i, desc, ctx,
							(const void **)&buildinfo, &mod_manifest);

			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

bool comp_is_llext(struct comp_dev *comp)
{
	const uint32_t module_id = IPC4_MOD_ID(comp->ipc_config.id);
	const unsigned int base_module_id = LIB_MANAGER_GET_LIB_ID(module_id) <<
		LIB_MANAGER_LIB_ID_SHIFT;
	const struct sof_man_module *mod = lib_manager_get_module_manifest(base_module_id);

	return mod && module_is_llext(mod);
}

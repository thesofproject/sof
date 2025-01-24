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

static int llext_manager_load_data_from_storage(const struct llext *ext,
						void __sparse_cache *vma,
						const uint8_t *load_base,
						size_t size, uint32_t flags)
{
	unsigned int i;
	int ret = llext_manager_align_map(vma, size, SYS_MM_MEM_PERM_RW);
	const elf_shdr_t *shdr;

	if (ret < 0) {
		tr_err(&lib_manager_tr, "cannot map %u of %p", size, (__sparse_force void *)vma);
		return ret;
	}

	size_t init_offset = 0;

	/* Need to copy sections within regions individually, offsets may differ */
	for (i = 0, shdr = llext_section_headers(ext); i < llext_section_count(ext); i++, shdr++) {
		if ((uintptr_t)shdr->sh_addr < (uintptr_t)vma ||
		    (uintptr_t)shdr->sh_addr >= (uintptr_t)vma + size)
			continue;

		if (!init_offset)
			init_offset = shdr->sh_offset;

		/* found a section within the region */
		size_t offset = shdr->sh_offset - init_offset;

		ret = memcpy_s((__sparse_force void *)shdr->sh_addr, size - offset,
			       load_base + offset, shdr->sh_size);
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

static int llext_manager_load_module(const struct llext *ext, const struct lib_manager_module *mctx)
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

	/* Copy Code */
	ret = llext_manager_load_data_from_storage(ext, va_base_text, ext->mem[LLEXT_MEM_TEXT],
						   text_size, SYS_MM_MEM_PERM_EXEC);
	if (ret < 0)
		return ret;

	/* Copy read-only data */
	ret = llext_manager_load_data_from_storage(ext, va_base_rodata, ext->mem[LLEXT_MEM_RODATA],
						   rodata_size, 0);
	if (ret < 0)
		goto e_text;

	/* Copy writable data */
	ret = llext_manager_load_data_from_storage(ext, va_base_data, ext->mem[LLEXT_MEM_DATA],
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

static int llext_manager_unload_module(const struct lib_manager_module *mctx)
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

	return err;
}

static bool llext_manager_section_detached(const elf_shdr_t *shdr)
{
	return shdr->sh_addr < SOF_MODULE_DRAM_LINK_END;
}

static int llext_manager_add_dependency(struct llext_loader *loader,
					struct lib_manager_module *mctx)
{
	if (mctx->n_depend)
		return 0;

	unsigned int n_dep;

	for (n_dep = 0; n_dep < LLEXT_MAX_DEPENDENCIES && loader->dependency[n_dep][0]; n_dep++)
		;

	if (!n_dep)
		return 0;

	void *dep = rmalloc(SOF_MEM_ZONE_RUNTIME_SHARED, SOF_MEM_FLAG_COHERENT,
			    SOF_MEM_CAPS_RAM, n_dep * LLEXT_MAX_NAME_LENGTH);

	if (!dep)
		return -ENOMEM;

	memcpy(dep, loader->dependency, n_dep * LLEXT_MAX_NAME_LENGTH);

	mctx->depend = dep;
	mctx->n_depend = n_dep;

	return 0;
}

static void llext_manager_restore_dependency(struct llext_loader *loader,
					     struct lib_manager_module *mctx)
{
	if (mctx->n_depend)
		memcpy(loader->dependency, mctx->depend, mctx->n_depend * LLEXT_MAX_NAME_LENGTH);
}

static int llext_manager_link(struct llext_buf_loader *ebl, const char *name,
			      struct lib_manager_module *mctx, const void **buildinfo,
			      const struct sof_man_module_manifest **mod_manifest)
{
	struct llext **llext = &mctx->llext;
	/* Identify if this is the first time loading this module */
	struct llext_load_param ldr_parm = {
		.relocate_local = !mctx->segment[LIB_MANAGER_TEXT].size,
		.pre_located = true,
		.section_detached = llext_manager_section_detached,
	};

	llext_manager_restore_dependency(&ebl->loader, mctx);

	int ret = llext_load(&ebl->loader, name, llext, &ldr_parm);

	if (ret)
		return ret;

	ret = llext_manager_add_dependency(&ebl->loader, mctx);
	if (ret < 0)
		return ret;

	mctx->segment[LIB_MANAGER_TEXT].addr = ebl->loader.sects[LLEXT_MEM_TEXT].sh_addr;
	mctx->segment[LIB_MANAGER_TEXT].size = ebl->loader.sects[LLEXT_MEM_TEXT].sh_size;

	tr_dbg(&lib_manager_tr, ".text: start: %#lx size %#x",
	       mctx->segment[LIB_MANAGER_TEXT].addr,
	       mctx->segment[LIB_MANAGER_TEXT].size);

	/* All read-only data sections */
	mctx->segment[LIB_MANAGER_RODATA].addr =
		ebl->loader.sects[LLEXT_MEM_RODATA].sh_addr;
	mctx->segment[LIB_MANAGER_RODATA].size = ebl->loader.sects[LLEXT_MEM_RODATA].sh_size;

	tr_dbg(&lib_manager_tr, ".rodata: start: %#lx size %#x",
	       mctx->segment[LIB_MANAGER_RODATA].addr,
	       mctx->segment[LIB_MANAGER_RODATA].size);

	/* All writable data sections */
	mctx->segment[LIB_MANAGER_DATA].addr =
		ebl->loader.sects[LLEXT_MEM_DATA].sh_addr;
	mctx->segment[LIB_MANAGER_DATA].size = ebl->loader.sects[LLEXT_MEM_DATA].sh_size;

	tr_dbg(&lib_manager_tr, ".data: start: %#lx size %#x",
	       mctx->segment[LIB_MANAGER_DATA].addr,
	       mctx->segment[LIB_MANAGER_DATA].size);

	mctx->segment[LIB_MANAGER_BSS].addr = ebl->loader.sects[LLEXT_MEM_BSS].sh_addr;
	mctx->segment[LIB_MANAGER_BSS].size = ebl->loader.sects[LLEXT_MEM_BSS].sh_size;

	tr_dbg(&lib_manager_tr, ".bss: start: %#lx size %#x",
	       mctx->segment[LIB_MANAGER_BSS].addr,
	       mctx->segment[LIB_MANAGER_BSS].size);

	ssize_t binfo_o = llext_find_section(&ebl->loader, ".mod_buildinfo");

	if (binfo_o >= 0)
		*buildinfo = llext_peek(&ebl->loader, binfo_o);

	ssize_t mod_o = llext_find_section(&ebl->loader, ".module");

	if (mod_o >= 0)
		*mod_manifest = llext_peek(&ebl->loader, mod_o);

	return binfo_o >= 0 && mod_o >= 0 ? 0 : -EPROTO;
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
			ctx->mod[n_mod].n_depend = 0;
			ctx->mod[n_mod].segment[LIB_MANAGER_TEXT].size = 0;
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

static void llext_depend_unlink(struct lib_manager_module *dep_ctx[], int n)
{
	for (; n >= 0; n--)
		if (dep_ctx[n] && dep_ctx[n]->llext->use_count == 1)
			llext_manager_unload_module(dep_ctx[n]);
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

	uintptr_t dram_base = (uintptr_t)desc - SOF_MAN_ELF_TEXT_OFFSET;
	struct llext_buf_loader ebl = LLEXT_BUF_LOADER((uint8_t *)dram_base + mod_offset, mod_size);

	/*
	 * LLEXT linking is only needed once for all the "drivers" in the
	 * module. This calls llext_load(), which also takes references to any
	 * dependencies, sets up sections and retrieves buildinfo and
	 * mod_manifest
	 */
	ret = llext_manager_link(&ebl, mod_array[entry_index - inst_idx].name, mctx,
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

uintptr_t llext_manager_allocate_module(const struct comp_ipc_config *ipc_config,
					const void *ipc_specific_config)
{
	uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	/* Library manifest */
	struct sof_man_fw_desc *desc = (struct sof_man_fw_desc *)
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
	int ret, i;

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

			ret = llext_manager_load_module(dep_ctx[i]->llext, dep_ctx[i]);
			if (ret < 0) {
				llext_depend_unlink(dep_ctx, i - 1);
				return 0;
			}
		}

		/* Map executable code and data */
		ret = llext_manager_load_module(mctx->llext, mctx);
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

	struct lib_manager_module *dep_ctx[LLEXT_MAX_DEPENDENCIES] = {};
	int dep_cnt;

	for (dep_cnt = 0; dep_cnt < ARRAY_SIZE(mctx->llext->dependency); dep_cnt++)
		if (llext_lib_find(mctx->llext->dependency[dep_cnt], &dep_ctx[dep_cnt]) < 0)
			break;

	if (llext_unload(&mctx->llext))
		/* More users are active */
		return 0;

	tr_dbg(&lib_manager_tr, "mod_id: %#x", component_id);

	/* Last user cleaning up, put dependencies */
	if (dep_cnt > 0)
		llext_depend_unlink(dep_ctx, dep_cnt - 1);

	return llext_manager_unload_module(mctx);
}

/* A library has been loaded, need to initialize its context */
int llext_manager_add_library(uint32_t lib_id)
{
	uint32_t module_id = lib_id << LIB_MANAGER_LIB_ID_SHIFT;
	struct lib_manager_mod_ctx *const ctx = lib_manager_get_mod_ctx(module_id);
	const struct sof_man_fw_desc *desc = lib_manager_get_library_manifest(module_id);
	unsigned int i;

	if (ctx->mod) {
		tr_err(&lib_manager_tr, "lib_id: %u: repeated load!", lib_id);
		return -EBUSY;
	}

	int ret = llext_manager_mod_init(ctx, desc);

	if (ret < 0)
		return ret;

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

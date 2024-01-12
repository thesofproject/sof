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
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/modules.h>

#include <zephyr/cache.h>
#include <zephyr/drivers/mm/system_mm.h>

#if CONFIG_LIBRARY_AUTH_SUPPORT
#include <sof/auth/auth_api.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(lib_manager, CONFIG_SOF_LOG_LEVEL);
/* 54cf5598-8b29-11ec-a8a3-0242ac120002 */

DECLARE_SOF_UUID("lib_manager", lib_manager_uuid, 0x54cf5598, 0x8b29, 0x11ec,
		 0xa8, 0xa3, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02);

DECLARE_TR_CTX(lib_manager_tr, SOF_UUID(lib_manager_uuid), LOG_LEVEL_INFO);

struct lib_manager_dma_ext {
	struct dma *dma;
	struct dma_chan_data *chan;
	uintptr_t dma_addr;		/**< buffer start pointer */
	uint32_t addr_align;
};

static struct ext_library loader_ext_lib;

#if CONFIG_LIBRARY_AUTH_SUPPORT
static int lib_manager_auth_init(void)
{
	struct ext_library *ext_lib = ext_lib_get();
	int ret;

	if (auth_api_version().major != AUTH_API_VERSION_MAJOR) {
		return -EINVAL;
	}

	ext_lib->auth_buffer = rballoc_align(0, SOF_MEM_CAPS_RAM,
					AUTH_SCRATCH_BUFF_SZ, CONFIG_MM_DRV_PAGE_SIZE);
	if (!ext_lib->auth_buffer)
		return -ENOMEM;


	ret = auth_api_init(&ext_lib->auth_ctx, ext_lib->auth_buffer,
				AUTH_SCRATCH_BUFF_SZ, IMG_TYPE_LIB);
	if (ret != 0) {
		tr_err(&lib_manager_tr, "lib_manager_auth_init() failed with error: %d", ret);
		ret = -EACCES;
	}

	return ret;
}

static void lib_manager_auth_deinit(void)
{
	struct ext_library *ext_lib = ext_lib_get();

	if (ext_lib->auth_buffer)
		rfree(ext_lib->auth_buffer);
	ext_lib->auth_buffer = NULL;
	memset(&ext_lib->auth_ctx, 0, sizeof(struct auth_api_ctx));
}

static int lib_manager_auth_proc(const void *buffer_data,
				size_t buffer_size, enum auth_phase phase)
{
	struct ext_library *ext_lib = ext_lib_get();
	int ret;

	ret = auth_api_init_auth_proc(&ext_lib->auth_ctx, buffer_data, buffer_size, phase);

	if (ret != 0) {
		tr_err(&lib_manager_tr, "lib_manager_auth_proc() failed with error: %d", ret);
		return -EACCES;
	}

	/* The auth_api_busy() will timeouts internally in case of failure */
	while(auth_api_busy(&ext_lib->auth_ctx));

	ret = auth_api_result(&ext_lib->auth_ctx);

	if (ret != AUTH_IMAGE_TRUSTED) {
	  tr_err(&lib_manager_tr, "lib_manager_auth_proc() Untrasted library!");
		return -ENOTSUP;
	}

	if (phase == AUTH_PHASE_LAST) {
		auth_api_cleanup(&ext_lib->auth_ctx);
	}

	return 0;
}
#endif /* CONFIG_LIBRARY_AUTH_SUPPORT */


#if IS_ENABLED(CONFIG_MM_DRV)

#define PAGE_SZ		CONFIG_MM_DRV_PAGE_SIZE

static int lib_manager_load_data_from_storage(void __sparse_cache *vma, void *s_addr,
					      uint32_t size, uint32_t flags)
{
	int ret = sys_mm_drv_map_region((__sparse_force void *)vma, POINTER_TO_UINT(NULL),
					size, flags);
	if (ret < 0)
		return ret;

	ret = memcpy_s((__sparse_force void *)vma, size, s_addr, size);
	if (ret < 0)
		return ret;

	dcache_writeback_region(vma, size);

	/* TODO: Change attributes for memory to FLAGS  */
	return 0;
}

static int lib_manager_load_module(uint32_t module_id, struct sof_man_module *mod,
				   struct sof_man_fw_desc *desc)
{
	struct ext_library *ext_lib = ext_lib_get();
	uint32_t lib_id = LIB_MANAGER_GET_LIB_ID(module_id);
	size_t load_offset = (size_t)((void *)ext_lib->desc[lib_id]);
	void __sparse_cache *va_base_text = (void __sparse_cache *)
		mod->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr;
	void *src_txt = (void *)(mod->segment[SOF_MAN_SEGMENT_TEXT].file_offset + load_offset);
	size_t st_text_size = mod->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length;
	void __sparse_cache *va_base_rodata = (void __sparse_cache *)
		mod->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr;
	void *src_rodata =
		(void *)(mod->segment[SOF_MAN_SEGMENT_RODATA].file_offset + load_offset);
	size_t st_rodata_size = mod->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length;
	int ret;

	st_text_size = st_text_size * PAGE_SZ;
	st_rodata_size = st_rodata_size * PAGE_SZ;

	/* Copy Code */
	ret = lib_manager_load_data_from_storage(va_base_text, src_txt, st_text_size,
						 SYS_MM_MEM_PERM_RW | SYS_MM_MEM_PERM_EXEC);
	if (ret < 0)
		goto err;

	/* Copy RODATA */
	ret = lib_manager_load_data_from_storage(va_base_rodata, src_rodata,
						 st_rodata_size, SYS_MM_MEM_PERM_RW);
	if (ret < 0)
		goto err;

	/* There are modules marked as lib_code. This is code shared between several modules inside
	 * the library. Load all lib_code modules with first none lib_code module load.
	 */
	if (!mod->type.lib_code)
		ext_lib->mods_exec_load_cnt++;

	if (ext_lib->mods_exec_load_cnt == 1) {
		struct sof_man_module *module_entry =
		    (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(0));
		for (size_t idx = 0; idx < desc->header.num_module_entries;
				++idx, ++module_entry) {
			if (module_entry->type.lib_code) {
				ret = lib_manager_load_module(lib_id << LIB_MANAGER_LIB_ID_SHIFT |
							      idx, mod, desc);
				if (ret < 0)
					goto err;
			}
		}
	}

	return 0;

err:
	sys_mm_drv_unmap_region((__sparse_force void *)va_base_text, st_text_size);
	sys_mm_drv_unmap_region((__sparse_force void *)va_base_rodata, st_rodata_size);

	return ret;
}

static int lib_manager_unload_module(uint32_t module_id, struct sof_man_module *mod,
				     struct sof_man_fw_desc *desc)
{
	struct ext_library *ext_lib = ext_lib_get();
	uint32_t lib_id = LIB_MANAGER_GET_LIB_ID(module_id);
	void __sparse_cache *va_base_text = (void __sparse_cache *)
		mod->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr;
	size_t st_text_size = mod->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length;
	void __sparse_cache *va_base_rodata = (void __sparse_cache *)
		mod->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr;
	size_t st_rodata_size = mod->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length;
	int ret;

	st_text_size = st_text_size * PAGE_SZ;
	st_rodata_size = st_rodata_size * PAGE_SZ;

	ret = sys_mm_drv_unmap_region((__sparse_force void *)va_base_text, st_text_size);
	if (ret < 0)
		return ret;

	ret = sys_mm_drv_unmap_region((__sparse_force void *)va_base_rodata, st_rodata_size);
	if (ret < 0)
		return ret;

	/* There are modules marked as lib_code. This is code shared between several modules inside
	 * the library. Unload all lib_code modules with last none lib_code module unload.
	 */
	if (mod->type.lib_code)
		return ret;

	if (!mod->type.lib_code && ext_lib->mods_exec_load_cnt > 0)
		ext_lib->mods_exec_load_cnt--;

	if (ext_lib->mods_exec_load_cnt == 0) {
		struct sof_man_module *module_entry =
		    (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(0));
		for (size_t idx = 0; idx < desc->header.num_module_entries;
				++idx, ++module_entry) {
			if (module_entry->type.lib_code) {
				ret =
				lib_manager_unload_module(lib_id << LIB_MANAGER_LIB_ID_SHIFT |
							  idx, mod, desc);
			}
		}
	}

	return ret;
}

static void __sparse_cache *lib_manager_get_instance_bss_address(uint32_t module_id,
								 uint32_t instance_id,
								 struct sof_man_module *mod)
{
	uint32_t instance_bss_size =
		 mod->segment[SOF_MAN_SEGMENT_BSS].flags.r.length / mod->instance_max_count;
	uint32_t inst_offset = instance_bss_size * PAGE_SZ * instance_id;
	void __sparse_cache *va_base =
		(void __sparse_cache *)(mod->segment[SOF_MAN_SEGMENT_BSS].v_base_addr +
					inst_offset);

	tr_dbg(&lib_manager_tr,
	       "lib_manager_get_instance_bss_address(): instance_bss_size: %#x, pointer: %p",
	       instance_bss_size, (__sparse_force void *)va_base);

	return va_base;
}

static int lib_manager_allocate_module_instance(uint32_t module_id, uint32_t instance_id,
						uint32_t is_pages, struct sof_man_module *mod)
{
	uint32_t bss_size =
			(mod->segment[SOF_MAN_SEGMENT_BSS].flags.r.length / mod->instance_max_count)
			 * PAGE_SZ;
	void __sparse_cache *va_base = lib_manager_get_instance_bss_address(module_id,
									    instance_id, mod);

	if ((is_pages * PAGE_SZ) > bss_size) {
		tr_err(&lib_manager_tr,
		       "lib_manager_allocate_module_instance(): invalid is_pages: %u, required: %u",
		       is_pages, bss_size / PAGE_SZ);
		return -ENOMEM;
	}

	/*
	 * Map bss memory and clear it.
	 */
	if (sys_mm_drv_map_region((__sparse_force void *)va_base, POINTER_TO_UINT(NULL),
				  bss_size, SYS_MM_MEM_PERM_RW) < 0)
		return -ENOMEM;

	memset((__sparse_force void *)va_base, 0, bss_size);

	return 0;
}

static int lib_manager_free_module_instance(uint32_t module_id, uint32_t instance_id,
					    struct sof_man_module *mod)
{
	uint32_t bss_size =
			(mod->segment[SOF_MAN_SEGMENT_BSS].flags.r.length / mod->instance_max_count)
			 * PAGE_SZ;
	void __sparse_cache *va_base = lib_manager_get_instance_bss_address(module_id,
									    instance_id, mod);
	/*
	 * Unmap bss memory.
	 */
	return sys_mm_drv_unmap_region((__sparse_force void *)va_base, bss_size);
}

uint32_t lib_manager_allocate_module(const struct comp_driver *drv,
				     struct comp_ipc_config *ipc_config,
				     const void *ipc_specific_config)
{
	struct sof_man_fw_desc *desc;
	struct sof_man_module *mod;
	const struct ipc4_base_module_cfg *base_cfg = ipc_specific_config;
	int ret;
	uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);

	tr_dbg(&lib_manager_tr, "lib_manager_allocate_module(): mod_id: %#x",
	       ipc_config->id);

	desc = lib_manager_get_library_module_desc(module_id);
	if (!desc) {
		tr_err(&lib_manager_tr,
		       "lib_manager_allocate_module(): failed to get module descriptor");
		return 0;
	}

	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(entry_index));

	ret = lib_manager_load_module(module_id, mod, desc);
	if (ret < 0)
		return 0;

	ret = lib_manager_allocate_module_instance(module_id, IPC4_INST_ID(ipc_config->id),
						   base_cfg->is_pages, mod);
	if (ret < 0) {
		tr_err(&lib_manager_tr,
		       "lib_manager_allocate_module(): module allocation failed: %d", ret);
		return 0;
	}
	return mod->entry_point;
}

int lib_manager_free_module(const struct comp_driver *drv,
			    struct comp_ipc_config *ipc_config)
{
	struct sof_man_fw_desc *desc;
	struct sof_man_module *mod;
	uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);
	int ret;

	tr_dbg(&lib_manager_tr, "lib_manager_free_module(): mod_id: %#x", ipc_config->id);

	desc = lib_manager_get_library_module_desc(module_id);
	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(entry_index));

	ret = lib_manager_unload_module(module_id, mod, desc);
	if (ret < 0)
		return ret;

	ret = lib_manager_free_module_instance(module_id, IPC4_INST_ID(ipc_config->id), mod);
	if (ret < 0) {
		tr_err(&lib_manager_tr,
		       "lib_manager_free_module(): free module instance failed: %d", ret);
		return ret;
	}
	return 0;
}

#else /* CONFIG_MM_DRV */

#define PAGE_SZ		4096 /* equals to MAN_PAGE_SIZE used by rimage */

uint32_t lib_manager_allocate_module(const struct comp_driver *drv,
				     struct comp_ipc_config *ipc_config,
				     const void *ipc_specific_config)
{
	tr_err(&lib_manager_tr,
	       "lib_manager_allocate_module(): Dynamic module allocation is not supported");
	return 0;
}

int lib_manager_free_module(const struct comp_driver *drv,
			    struct comp_ipc_config *ipc_config)
{
	/* Since we cannot allocate the freeing is not considered to be an error */
	tr_warn(&lib_manager_tr,
		"lib_manager_free_module(): Dynamic module freeing is not supported");
	return 0;
}
#endif /* CONFIG_MM_DRV */

void lib_manager_init(void)
{
	struct sof *sof = sof_get();

	if (!sof->ext_library)
		sof->ext_library = &loader_ext_lib;
}

struct sof_man_fw_desc *lib_manager_get_library_module_desc(int module_id)
{
	uint32_t lib_id = LIB_MANAGER_GET_LIB_ID(module_id);
	struct ext_library *_ext_lib = ext_lib_get();
	uint8_t *buffptr = (uint8_t *)_ext_lib->desc[lib_id];

	if (!buffptr)
		return NULL;
	return (struct sof_man_fw_desc *)(buffptr + SOF_MAN_ELF_TEXT_OFFSET);
}

static void lib_manager_update_sof_ctx(struct sof_man_fw_desc *desc, uint32_t lib_id)
{
	struct ext_library *_ext_lib = ext_lib_get();

	_ext_lib->desc[lib_id] = desc;
	/* TODO: maybe need to call here dcache_writeback here? */
}

#if CONFIG_INTEL_MODULES
int lib_manager_register_module(struct sof_man_fw_desc *desc, int module_id)
{
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);
	/* allocate new  comp_driver_info */
	struct comp_driver_info *new_drv_info;
	struct comp_driver *drv = NULL;
	struct sof_man_module *mod;
	int ret;

	new_drv_info = rmalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0,
			       SOF_MEM_CAPS_RAM | SOF_MEM_FLAG_COHERENT,
			       sizeof(struct comp_driver_info));

	if (!new_drv_info) {
		tr_err(&lib_manager_tr,
		       "lib_manager_register_module(): failed to allocate comp_driver_info");
		ret = -ENOMEM;
		goto cleanup;
	}

	drv = rmalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0,
		      SOF_MEM_CAPS_RAM | SOF_MEM_FLAG_COHERENT,
		      sizeof(struct comp_driver));
	if (!drv) {
		tr_err(&lib_manager_tr,
		       "lib_manager_register_module(): failed to allocate comp_driver");
		ret = -ENOMEM;
		goto cleanup;
	}
	memset(drv, 0, sizeof(struct comp_driver));

	/* Fill the new_drv_info structure with already known parameters */
	/* Check already registered components */
	mod = (struct sof_man_module *)((uint8_t *)desc + SOF_MAN_MODULE_OFFSET(entry_index));
	struct sof_uuid *uid = (struct sof_uuid *)&mod->uuid[0];

	declare_dynamic_module_adapter(drv, SOF_COMP_MODULE_ADAPTER, uid, &lib_manager_tr);

	new_drv_info->drv = drv;

	/* Register new driver in the list */
	ret = comp_register(new_drv_info);

cleanup:
	if (ret < 0) {
		if (drv)
			rfree(drv);
		if (new_drv_info)
			rfree(new_drv_info);
	}

	return ret;
}

#else /* CONFIG_INTEL_MODULES */
int lib_manager_register_module(struct sof_man_fw_desc *desc, int module_id)
{
	tr_err(&lib_manager_tr,
	       "lib_manager_register_module(): Dynamic module loading is not supported");
	return -ENOTSUP;
}
#endif /* CONFIG_INTEL_MODULES */

static int lib_manager_dma_buffer_alloc(struct lib_manager_dma_ext *dma_ext,
					uint32_t size)
{
	/*
	 * allocate new buffer: this is the actual DMA buffer but we
	 * traditionally allocate a cached address for it
	 */
	dma_ext->dma_addr = (uintptr_t)rballoc_align(0, SOF_MEM_CAPS_DMA, size,
						     dma_ext->addr_align);
	if (!dma_ext->dma_addr) {
		tr_err(&lib_manager_tr, "lib_manager_dma_buffer_alloc(): alloc failed");
		return -ENOMEM;
	}

	dcache_invalidate_region((void __sparse_cache *)dma_ext->dma_addr, size);

	tr_dbg(&lib_manager_tr,
	       "lib_manager_dma_buffer_alloc(): address: %#lx, size: %u",
	       dma_ext->dma_addr, size);

	return 0;
}

/**
 * \brief Start, init and alloc DMA and buffer used by loader.
 *
 * \return 0 on success, error code otherwise.
 */
static int lib_manager_dma_init(struct lib_manager_dma_ext *dma_ext, uint32_t dma_id)
{
	int chan_index;

	/* Initialize dma_ext with zeros */
	memset(dma_ext, 0, sizeof(struct lib_manager_dma_ext));
	/* request DMA in the dir HMEM->LMEM */
	dma_ext->dma = dma_get(DMA_DIR_HMEM_TO_LMEM, 0, DMA_DEV_HOST,
			       DMA_ACCESS_EXCLUSIVE);
	if (!dma_ext->dma) {
		tr_err(&lib_manager_tr,
		       "lib_manager_dma_init(): dma_ext->dma = NULL");
		return -ENODEV;
	}

	chan_index = dma_request_channel(dma_ext->dma->z_dev, &dma_id);
	dma_ext->chan = &dma_ext->dma->chan[chan_index];
	if (!dma_ext->chan)
		return -EINVAL;

	return 0;
}

/**
 * \brief Stop, deinit and free DMA and buffer used by loader.
 *
 * \return 0 on success, error code otherwise.
 */

static int lib_manager_dma_deinit(struct lib_manager_dma_ext *dma_ext, uint32_t dma_id)
{
	if (dma_ext->dma) {
		if (dma_ext->dma->z_dev)
			dma_release_channel(dma_ext->dma->z_dev, dma_id);

		dma_put(dma_ext->dma);
	}
	return 0;
}

static int lib_manager_load_data_from_host(struct lib_manager_dma_ext *dma_ext, uint32_t size)
{
	uint64_t timeout = k_ms_to_cyc_ceil64(200);
	struct dma_status stat;
	int ret;

	/* Wait till whole data acquired with timeout of 200ms */
	timeout += sof_cycle_get_64();

	for (;;) {
		ret = dma_get_status(dma_ext->chan->dma->z_dev,
				     dma_ext->chan->index, &stat);
		if (ret < 0 || stat.pending_length >= size)
			return ret;

		if (sof_cycle_get_64() > timeout)
			break;

		k_usleep(100);
	}

	tr_err(&lib_manager_tr,
	       "lib_manager_load_data_from_host(): timeout during DMA transfer");
	return -ETIMEDOUT;
}

static int lib_manager_store_data(struct lib_manager_dma_ext *dma_ext,
				  void __sparse_cache *dst_addr, uint32_t dst_size)
{
	uint32_t copied_bytes = 0;

	while (copied_bytes < dst_size) {
		uint32_t bytes_to_copy;
		int ret;

		if ((dst_size - copied_bytes) > MAN_MAX_SIZE_V1_8)
			bytes_to_copy = MAN_MAX_SIZE_V1_8;
		else
			bytes_to_copy = dst_size - copied_bytes;

		ret = lib_manager_load_data_from_host(dma_ext, bytes_to_copy);
		if (ret < 0)
			return ret;
		dcache_invalidate_region((void __sparse_cache *)dma_ext->dma_addr, bytes_to_copy);
		memcpy_s((__sparse_force uint8_t *)dst_addr + copied_bytes, bytes_to_copy,
			 (void *)dma_ext->dma_addr, bytes_to_copy);
		copied_bytes += bytes_to_copy;
		dma_reload(dma_ext->chan->dma->z_dev, dma_ext->chan->index, 0, 0, bytes_to_copy);
	}

	return 0;
}

static void __sparse_cache *lib_manager_allocate_store_mem(uint32_t size,
							   uint32_t attribs)
{
	void __sparse_cache *local_add;
#if CONFIG_L3_HEAP
	uint32_t caps = SOF_MEM_CAPS_L3 | SOF_MEM_CAPS_DMA;

	/* allocate new buffer: cached alias */
	local_add = (__sparse_force void __sparse_cache *)rmalloc(SOF_MEM_ZONE_SYS, 0, caps, size);
#else
	uint32_t addr_align = PAGE_SZ;
	uint32_t caps = SOF_MEM_CAPS_DMA;

	/* allocate new buffer: cached alias */
	local_add = (__sparse_force void __sparse_cache *)rballoc_align(0, caps, size, addr_align);
#endif
	if (!local_add) {
		tr_err(&lib_manager_tr, "lib_manager_allocate_store_mem(): alloc failed");
		return NULL;
	}

	dcache_invalidate_region(local_add, size);
	icache_invalidate_region(local_add, size);

	return local_add;
}

static int lib_manager_store_library(struct lib_manager_dma_ext *dma_ext,
				     void __sparse_cache *man_buffer,
				     uint32_t lib_id)
{
	void __sparse_cache *library_base_address;
	struct sof_man_fw_desc *man_desc = (struct sof_man_fw_desc *)
		((__sparse_force uint8_t *)man_buffer + SOF_MAN_ELF_TEXT_OFFSET);
	uint32_t preload_size = man_desc->header.preload_page_count * PAGE_SZ;
	int ret;

	/* Prepare storage memory, note: it is never freed, library unloading is unsupported */
	library_base_address = lib_manager_allocate_store_mem(preload_size, 0);
	if (!library_base_address)
		return -ENOMEM;

	tr_dbg(&lib_manager_tr, "lib_manager_store_library(): pointer: %p",
	       (__sparse_force void *)library_base_address);

#if CONFIG_LIBRARY_AUTH_SUPPORT
	ret = lib_manager_auth_proc(man_buffer,
				MAN_MAX_SIZE_V1_8, AUTH_PHASE_FIRST);
	if (ret < 0)
		return ret;
#endif /* CONFIG_LIBRARY_AUTH_SUPPORT */

	/* Copy data from temp_mft_buf to destination memory (pointed by library_base_address) */
	memcpy_s((__sparse_force void *)library_base_address, MAN_MAX_SIZE_V1_8,
		 (__sparse_force void *)man_buffer, MAN_MAX_SIZE_V1_8);

	/* Copy remaining library part into storage buffer */
	ret = lib_manager_store_data(dma_ext, (uint8_t __sparse_cache *)library_base_address +
				     MAN_MAX_SIZE_V1_8, preload_size - MAN_MAX_SIZE_V1_8);
	if (ret < 0) {
		rfree((__sparse_force void *)library_base_address);
		return ret;
	}

#if CONFIG_LIBRARY_AUTH_SUPPORT
	ret = lib_manager_auth_proc((void *)library_base_address,
				preload_size - MAN_MAX_SIZE_V1_8, AUTH_PHASE_LAST);
	if (ret < 0)
		return ret;
#endif /* CONFIG_LIBRARY_AUTH_SUPPORT */

	/* Now update sof context with new library */
	lib_manager_update_sof_ctx((__sparse_force void *)library_base_address, lib_id);

	return 0;
}

static int lib_manager_setup(uint32_t dma_id)
{
	struct ext_library *_ext_lib = ext_lib_get();
	struct lib_manager_dma_ext *dma_ext;
	struct dma_block_config dma_block_cfg = {
		.block_size = MAN_MAX_SIZE_V1_8,
		.flow_control_mode = 1,
	};
	struct dma_config config = {
		.channel_direction = HOST_TO_MEMORY,
		.source_data_size = sizeof(uint32_t),
		.dest_data_size = sizeof(uint32_t),
		.block_count = 1,
		.head_block = &dma_block_cfg,
	};
	int ret;

	if (_ext_lib->runtime_data)
		return 0;

	dma_ext = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			  sizeof(*dma_ext));
	if (!dma_ext)
		return -ENOMEM;

	ret = lib_manager_dma_init(dma_ext, dma_id);
	if (ret < 0)
		goto err_dma_init;

	ret = dma_get_attribute(dma_ext->dma->z_dev, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&dma_ext->addr_align);
	if (ret < 0)
		goto err_dma_init;

	ret = lib_manager_dma_buffer_alloc(dma_ext, MAN_MAX_SIZE_V1_8);
	if (ret < 0)
		goto err_dma_buffer;

	dma_block_cfg.dest_address = dma_ext->dma_addr;

	/*
	 * make sure that the DSP is running full speed for the duration of
	 * library loading
	 */
	ret = core_kcps_adjust(cpu_get_id(), CLK_MAX_CPU_HZ / 1000);
	if (ret < 0)
		goto err_dma_buffer;

	ret = dma_config(dma_ext->chan->dma->z_dev, dma_ext->chan->index, &config);
	if (ret < 0)
		goto err_dma;

	ret = dma_start(dma_ext->chan->dma->z_dev, dma_ext->chan->index);
	if (ret < 0)
		goto err_dma;

	_ext_lib->runtime_data = dma_ext;

	return 0;

err_dma:
	core_kcps_adjust(cpu_get_id(), -(CLK_MAX_CPU_HZ / 1000));

err_dma_buffer:
	lib_manager_dma_deinit(dma_ext, dma_id);

err_dma_init:
	rfree(dma_ext);
	return ret;
}

int lib_manager_load_library(uint32_t dma_id, uint32_t lib_id, uint32_t type)
{
	void __sparse_cache *man_tmp_buffer;
	struct lib_manager_dma_ext *dma_ext;
	struct ext_library *_ext_lib;
	int ret, ret2;

	if (type == SOF_IPC4_GLB_LOAD_LIBRARY &&
	    (lib_id == 0 || lib_id >= LIB_MANAGER_MAX_LIBS)) {
		tr_err(&lib_manager_tr,
		       "lib_manager_load_library(): invalid lib_id: %u", lib_id);
		return -EINVAL;
	}

	lib_manager_init();

	_ext_lib = ext_lib_get();

	if (type == SOF_IPC4_GLB_LOAD_LIBRARY_PREPARE || !_ext_lib->runtime_data) {
		ret = lib_manager_setup(dma_id);
		if (ret)
			return ret;

		if (type == SOF_IPC4_GLB_LOAD_LIBRARY_PREPARE)
			return 0;
	}

	dma_ext = _ext_lib->runtime_data;

	/* allocate temporary manifest buffer */
	man_tmp_buffer = (__sparse_force void __sparse_cache *)
			rballoc_align(0, SOF_MEM_CAPS_DMA,
				      MAN_MAX_SIZE_V1_8, CONFIG_MM_DRV_PAGE_SIZE);
	if (!man_tmp_buffer) {
		ret = -ENOMEM;
		goto cleanup;
	}

	/* Load manifest to temporary buffer. */
	ret = lib_manager_store_data(dma_ext, man_tmp_buffer, MAN_MAX_SIZE_V1_8);
	if (ret < 0)
		goto stop_dma;

#if CONFIG_LIBRARY_AUTH_SUPPORT
	/* Initialize authentication support */
	ret = lib_manager_auth_init();
	if (ret < 0)
		goto stop_dma;
#endif /* CONFIG_LIBRARY_AUTH_SUPPORT */

	ret = lib_manager_store_library(dma_ext, man_tmp_buffer, lib_id);

#if CONFIG_LIBRARY_AUTH_SUPPORT
	lib_manager_auth_deinit();
#endif /* CONFIG_LIBRARY_AUTH_SUPPORT */

stop_dma:
	ret2 = dma_stop(dma_ext->chan->dma->z_dev, dma_ext->chan->index);
	if (ret2 < 0) {
		tr_err(&lib_manager_tr,
		       "lib_manager_load_library(): error stopping DMA: %d", ret);
		if (!ret)
			ret = ret2;
	}

	rfree((__sparse_force void *)man_tmp_buffer);

cleanup:
	core_kcps_adjust(cpu_get_id(), -(CLK_MAX_CPU_HZ / 1000));
	rfree((void *)dma_ext->dma_addr);
	lib_manager_dma_deinit(dma_ext, dma_id);
	rfree(dma_ext);
	_ext_lib->runtime_data = NULL;

	if (!ret)
		tr_info(&ipc_tr, "loaded library id: %u", lib_id);

	return ret;
}

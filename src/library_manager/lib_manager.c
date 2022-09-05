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
#include <sof/ipc/topology.h>

#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <sof/lib_manager.h>
#include <sof/audio/module_adapter/module/generic.h>

#include <zephyr/drivers/mm/system_mm.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 54cf5598-8b29-11ec-a8a3-0242ac120002 */

DECLARE_SOF_UUID("lib_manager", lib_manager_uuid, 0x54cf5598, 0x8b29, 0x11ec,
		 0xa8, 0xa3, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02);

DECLARE_TR_CTX(lib_manager_tr, SOF_UUID(lib_manager_uuid), LOG_LEVEL_INFO);

/**
 * DMA buffer
 */
struct lib_manager_dma_buf {
	uintptr_t w_ptr;	/**< write pointer */
	uintptr_t r_ptr;	/**< read pointer */
	uintptr_t addr;		/**< buffer start pointer */
	uintptr_t end_addr;	/**< buffer end pointer */
	uint32_t size;		/**< buffer size */
	uint32_t avail;		/**< buffer avail data */
};

struct lib_manager_dma_ext {
	struct dma *dma;
	struct dma_chan_data *chan;
	struct lib_manager_dma_buf dmabp;
};

static struct ext_library loader_ext_lib;

static int lib_manager_load_data_from_storage(uint8_t *vma, uint8_t *s_addr,
					      uint32_t size, uint32_t flags)
{
	int ret = sys_mm_drv_map_region(vma, POINTER_TO_UINT(NULL),
					size, SYS_MM_MEM_PERM_RW | SYS_MM_MEM_PERM_EXEC);
	if (ret < 0)
		return ret;

	ret = memcpy_s((void *)vma, size, (void *)s_addr, size);
	if (ret < 0)
		return ret;

	dcache_writeback_region((void *)vma, size);

	/* TODO: Change attributes for memory to FLAGS  */
	return 0;
}

static int lib_manager_load_module(uint32_t module_id, struct sof_man_module *mod,
				   struct sof_man_fw_desc *desc)
{
	struct ext_library *ext_lib = ext_lib_get();
	uint32_t lib_id = LIB_MANAGER_GET_LIB_ID(module_id);
	size_t load_offset = (size_t)((void *)ext_lib->desc[lib_id]);
	void *va_base_text = (void *)mod->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr;
	void *src_txt = (void *)(mod->segment[SOF_MAN_SEGMENT_TEXT].file_offset + load_offset);
	size_t st_text_size = mod->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length;
	void *va_base_rodata = (void *)mod->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr;
	void *src_rodata =
		(void *)(mod->segment[SOF_MAN_SEGMENT_RODATA].file_offset + load_offset);
	size_t st_rodata_size = mod->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length;
	int ret;

	st_text_size = st_text_size * CONFIG_MM_DRV_PAGE_SIZE;
	st_rodata_size = st_rodata_size * CONFIG_MM_DRV_PAGE_SIZE;

	/* Copy Code */
	ret = lib_manager_load_data_from_storage(va_base_text, src_txt,
						 st_text_size, SYS_MM_MEM_PERM_EXEC);
	if (ret < 0)
		goto err;

	/* Copy RODATA */
	ret = lib_manager_load_data_from_storage(va_base_rodata, src_rodata,
						 st_rodata_size, SYS_MM_MEM_PERM_EXEC);
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
	sys_mm_drv_unmap_region(va_base_text, st_text_size);
	sys_mm_drv_unmap_region(va_base_rodata, st_rodata_size);

	return ret;
}

static int lib_manager_unload_module(uint32_t module_id, struct sof_man_module *mod,
				     struct sof_man_fw_desc *desc)
{
	struct ext_library *ext_lib = ext_lib_get();
	uint32_t lib_id = LIB_MANAGER_GET_LIB_ID(module_id);
	void *va_base_text = (void *)mod->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr;
	size_t st_text_size = mod->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length;
	void *va_base_rodata = (void *)mod->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr;
	size_t st_rodata_size = mod->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length;
	int ret;

	st_text_size = st_text_size * CONFIG_MM_DRV_PAGE_SIZE;
	st_rodata_size = st_rodata_size * CONFIG_MM_DRV_PAGE_SIZE;

	ret = sys_mm_drv_unmap_region(va_base_text, st_text_size);

	dcache_invalidate_region(va_base_text, st_text_size);

	ret = sys_mm_drv_unmap_region(va_base_rodata, st_rodata_size);

	dcache_invalidate_region(va_base_rodata, st_rodata_size);

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

static uint8_t *lib_manager_get_instance_bss_address(uint32_t module_id, uint32_t instance_id,
						     struct sof_man_module *mod)
{
	uint32_t instance_bss_size =
		 mod->segment[SOF_MAN_SEGMENT_BSS].flags.r.length / mod->instance_max_count;
	uint32_t inst_offset = instance_bss_size * CONFIG_MM_DRV_PAGE_SIZE * instance_id;
	uint8_t *va_base =
		(uint8_t *)(mod->segment[SOF_MAN_SEGMENT_BSS].v_base_addr + inst_offset);

	tr_dbg(&lib_manager_tr,
	       "lib_manager_get_instance_bss_address() instance_bss_size: 0x%x, pointer: 0x%p",
	       instance_bss_size, va_base);
	return va_base;
}

static int lib_manager_allocate_module_instance(uint32_t module_id, uint32_t instance_id,
						uint32_t is_pages, struct sof_man_module *mod)
{
	uint32_t bss_size =
			(mod->segment[SOF_MAN_SEGMENT_BSS].flags.r.length / mod->instance_max_count)
			 * CONFIG_MM_DRV_PAGE_SIZE;
	uint8_t *va_base = lib_manager_get_instance_bss_address(module_id, instance_id, mod);

	if ((is_pages * CONFIG_MM_DRV_PAGE_SIZE) > bss_size) {
		tr_err(&lib_manager_tr,  "is_pages (%d) invalid, required: %d",
		       is_pages, bss_size / CONFIG_MM_DRV_PAGE_SIZE);
		return -ENOMEM;
	}
	/*
	 * Map bss memory and clear it.
	 */
	if (sys_mm_drv_map_region(va_base, POINTER_TO_UINT(NULL),
				  bss_size, SYS_MM_MEM_PERM_RW) < 0)
		return -ENOMEM;
	memset(va_base, 0, bss_size);
	return 0;
}

static int lib_manager_free_module_instance(uint32_t module_id, uint32_t instance_id,
					    struct sof_man_module *mod)
{
	uint32_t bss_size =
			(mod->segment[SOF_MAN_SEGMENT_BSS].flags.r.length / mod->instance_max_count)
			 * CONFIG_MM_DRV_PAGE_SIZE;
	uint8_t *va_base = lib_manager_get_instance_bss_address(module_id, instance_id, mod);
	/*
	 * Unmap bss memory.
	 */
	return sys_mm_drv_unmap_region(va_base, bss_size);
}

uint32_t lib_manager_allocate_module(const struct comp_driver *drv,
				     struct comp_ipc_config *ipc_config,
				     void *ipc_specific_config)
{
	struct sof_man_fw_desc *desc;
	struct sof_man_module *mod;
	struct ipc4_base_module_cfg *base_cfg = ipc_specific_config;
	int ret;
	uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);

	tr_dbg(&lib_manager_tr, "lib_manager_allocate_module() mod_id: 0x%x", ipc_config->id);

	desc = lib_manager_get_library_module_desc(module_id);
	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(entry_index));

	ret = lib_manager_load_module(module_id, mod, desc);
	if (ret < 0)
		return 0;

	ret = lib_manager_allocate_module_instance(module_id, IPC4_INST_ID(ipc_config->id),
						   base_cfg->is_pages, mod);
	if (ret < 0) {
		tr_err(&lib_manager_tr, "lib_manager_allocate_module() failed: %d", ret);
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

	tr_dbg(&lib_manager_tr, "lib_manager_free_module() mod_id: 0x%x", ipc_config->id);

	desc = lib_manager_get_library_module_desc(module_id);
	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(entry_index));

	ret = lib_manager_unload_module(module_id, mod, desc);
	if (ret < 0)
		return ret;

	ret = lib_manager_free_module_instance(module_id, IPC4_INST_ID(ipc_config->id), mod);
	if (ret < 0) {
		tr_err(&lib_manager_tr, "lib_manager_allocate_module() failed: %d", ret);
		return ret;
	}
	return 0;
}

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

	return (struct sof_man_fw_desc *)(buffptr + SOF_MAN_ELF_TEXT_OFFSET);
}

void lib_manager_update_sof_ctx(struct sof_man_fw_desc *desc, uint32_t lib_id)
{
	struct ext_library *_ext_lib = ext_lib_get();

	_ext_lib->desc[lib_id] = desc;
	/* TODO: maybe need to call here dcache_writeback here? */
}

int lib_manager_register_module(struct sof_man_fw_desc *desc, int module_id)
{
	/* allocate new  comp_driver_info */
	struct comp_driver_info *new_drv_info;
	struct comp_driver *drv;
	struct sof_man_module *mod;
	int ret;

	uint32_t align = 4;
	uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);

	new_drv_info = rballoc_align(0, SOF_MEM_CAPS_RAM,
				     sizeof(struct comp_driver_info), align);

	if (!new_drv_info) {
		tr_err(&lib_manager_tr, "lib_manager_register_module(): alloc failed");
		ret = -ENOMEM;
		goto cleanup;
	}

	drv = rballoc_align(0, SOF_MEM_CAPS_RAM,
			    sizeof(struct comp_driver), align);
	if (!drv) {
		tr_err(&lib_manager_tr, "lib_manager_register_module(): alloc failed");
		ret = -ENOMEM;
		goto cleanup;
	}
	memset(drv, 0, sizeof(struct comp_driver));

	/* Fill the new_drv_info structure with already known parameters */
	/* Check already registered components */
	mod = (struct sof_man_module *)((uint8_t *)desc + SOF_MAN_MODULE_OFFSET(entry_index));
	struct sof_uuid *uid = (struct sof_uuid *)&mod->uuid[0];

#if CONFIG_INTEL_MODULES
	DECLARE_DYNAMIC_MODULE_ADAPTER(drv, SOF_COMP_MODULE_ADAPTER, *uid, lib_manager_tr);
#else
	ret = -ENOTSUP;
	goto cleanup;
#endif
	new_drv_info->drv = drv;

	/* Register new driver in the list */
	ret = comp_register(new_drv_info);

cleanup:
	if (ret < 0) {
		tr_err(&lib_manager_tr, "lib_manager_register_module() failed: %d", ret);
		if (drv)
			rfree(drv);
		if (new_drv_info)
			rfree(new_drv_info);
	}

	return ret;
}

static void lib_manager_dma_buffer_update(struct lib_manager_dma_buf *buffer, uintptr_t addr,
					  uint32_t size)
{
	buffer->size = size;
	buffer->addr = addr;
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + buffer->size;
	buffer->avail = 0;
}

static int lib_manager_dma_buffer_init(struct lib_manager_dma_buf *buffer, uint32_t size,
				       uint32_t align)
{
	/* allocate new buffer */
	buffer->addr = rballoc_align(0, SOF_MEM_CAPS_DMA, size, align);

	if (!buffer->addr) {
		tr_err(&lib_manager_tr, "dma_buffer_init(): alloc failed");
		return -ENOMEM;
	}

	bzero((__sparse_force void __sparse_cache *)buffer->addr, size);
	dcache_writeback_region((void *)buffer->addr, size);

	/* initialise the DMA buffer */

	lib_manager_dma_buffer_update(buffer, buffer->addr, size);
	tr_dbg(&lib_manager_tr, "lib_manager_dma_buffer_init(): 0x%x, 0x%x",
	       buffer->addr, buffer->end_addr);
	return 0;
}

void lib_manager_dma_buffer_free(struct lib_manager_dma_buf *buffer)
{
	rfree((void *)buffer->addr);
	memset(buffer, 0, sizeof(struct lib_manager_dma_buf));
}

/**
 * \brief Start, init and alloc DMA and buffer used by loader.
 *
 * \return 0 on success, error code otherwise.
 */
static int lib_manager_dma_init(struct lib_manager_dma_ext *dma_ext, uint32_t dma_id)
{
	uint32_t addr_align;
	int chan_index;
	int ret;

	/* request DMA in the dir HMEM->LMEM */
	dma_ext->dma = dma_get(DMA_DIR_HMEM_TO_LMEM, 0, DMA_DEV_HOST,
			       DMA_ACCESS_EXCLUSIVE);
	if (!dma_ext->dma) {
		tr_err(&lib_manager_tr, "dma_ext_init(): dma.dmac = NULL");
		return -ENODEV;
	}

	/* get required address alignment for dma buffer */
	ret = dma_get_attribute(dma_ext->dma, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (ret < 0)
		return ret;

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
	dma_release_channel(dma_ext->dma->z_dev, dma_id);
	dma_put(dma_ext->dma);
	return 0;
}

static int lib_manager_load_data_from_host(struct lib_manager_dma_ext *dma_ext, uint32_t size)
{
	struct dma_config config;
	struct dma_status stat;
	struct dma_block_config *dma_block_cfg;
	int ret;
	uint32_t avail_bytes = 0;

	config.channel_direction = HOST_TO_MEMORY;
	config.source_data_size = sizeof(uint32_t);
	config.dest_data_size = sizeof(uint32_t);
	config.block_count = 1;

	dma_block_cfg = rballoc(SOF_MEM_FLAG_COHERENT,
				SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
				sizeof(struct dma_block_config));
	if (!dma_block_cfg)
		return -ENOMEM;

	config.head_block = dma_block_cfg;
	dma_block_cfg->block_size = size;
	dma_block_cfg->dest_address = dma_ext->dmabp.addr;
	dma_block_cfg->flow_control_mode = 1;

	ret = dma_config(dma_ext->chan->dma->z_dev, dma_ext->chan->index, &config);

	if (ret < 0)
		goto return_on_fail;

	ret = dma_start(dma_ext->chan->dma->z_dev, dma_ext->chan->index);
	if (ret < 0)
		goto return_on_fail;

	/* Wait till whole data acquired */
	while (avail_bytes < size) {
		/* get data sizes from DMA */
		ret = dma_get_status(dma_ext->chan->dma->z_dev, dma_ext->chan->index, &stat);

		if (ret < 0)
			goto return_on_fail;

		avail_bytes = stat.pending_length;

		k_usleep(100);
	}
	ret = dma_stop(dma_ext->chan->dma->z_dev, dma_ext->chan->index);

	if (ret < 0)
		goto return_on_fail;
	ret = dma_reload(dma_ext->chan->dma->z_dev, dma_ext->chan->index, 0, 0, size);
	if (ret < 0)
		goto return_on_fail;

	dcache_invalidate_region((void *)dma_ext->dmabp.addr, size);

return_on_fail:
	rfree(dma_block_cfg);
	return ret;
}

static int lib_manager_store_data(struct lib_manager_dma_ext *dma_ext, void *dst_addr,
				  uint32_t dst_size)
{
	uint32_t copied_bytes = 0;
	uint32_t bytes_to_copy = 0;
	struct lib_manager_dma_buf *dma_buf = &dma_ext->dmabp;
	int ret;

	while (copied_bytes < dst_size) {
		if ((dst_size - copied_bytes) > MAN_MAX_SIZE_V1_8)
			bytes_to_copy = MAN_MAX_SIZE_V1_8;
		else
			bytes_to_copy = (dst_size - copied_bytes);

		lib_manager_dma_buffer_update(dma_buf, dma_ext->dmabp.addr,
					      bytes_to_copy);

		ret = lib_manager_load_data_from_host(dma_ext, bytes_to_copy);
		if (ret < 0)
			return ret;
		memcpy_s((void *)((char *)dst_addr + copied_bytes), bytes_to_copy,
			 (void *)dma_ext->dmabp.addr, bytes_to_copy);
		copied_bytes += bytes_to_copy;
	}
	dcache_writeback_region(dst_addr, dst_size);

	return 0;
}

static int lib_manager_allocate_store_mem(uint32_t *address, uint32_t size, uint32_t attribs)
{
	void *local_add;
#if CONFIG_L3_HEAP
	uint32_t caps = SOF_MEM_CAPS_L3 | SOF_MEM_CAPS_DMA;

	/* allocate new buffer */
	local_add = rmalloc(SOF_MEM_ZONE_SYS, 0, caps, size);
#else
	uint32_t addr_align = CONFIG_MM_DRV_PAGE_SIZE;
	uint32_t caps = SOF_MEM_CAPS_DMA;

	/* allocate new buffer */
	local_add = rballoc_align(0, caps, size, addr_align);
#endif
	if (!local_add) {
		tr_err(&lib_manager_tr, "dma_buffer_init(): alloc failed");
		return -ENOMEM;
	}

	*address = (uint32_t)local_add;
	return 0;
}

static int lib_manager_store_library(struct lib_manager_dma_ext *dma_ext, void *man_buffer,
				     uint32_t lib_id, uint32_t addr_align)
{
	uint32_t library_base_address;
	struct sof_man_fw_desc *man_desc =
			(struct sof_man_fw_desc *)((char *)man_buffer + SOF_MAN_ELF_TEXT_OFFSET);
	uint32_t preload_size = man_desc->header.preload_page_count * CONFIG_MM_DRV_PAGE_SIZE;
	int ret;

	/* Prepare storage memory */
	ret = lib_manager_allocate_store_mem((void *)&library_base_address, preload_size, 0);

	tr_err(&lib_manager_tr, "mm_map() :%d, pointer: 0x%x", ret, library_base_address);
	if (ret < 0)
		return ret;

	/* Copy data from temp_mft_buf to destination memory (pointed by library_base_address) */
	memcpy_s((void *)library_base_address, MAN_MAX_SIZE_V1_8,
		 (void *)man_buffer, MAN_MAX_SIZE_V1_8);

	dcache_writeback_invalidate_region((void *)library_base_address,
					   MAN_MAX_SIZE_V1_8);

	/* Copy remaining library part into storage buffer */
	ret = lib_manager_store_data(dma_ext,
				     (void *)(library_base_address + MAN_MAX_SIZE_V1_8),
				     (preload_size - MAN_MAX_SIZE_V1_8));
	if (ret < 0)
		return ret;

	/* Now update sof context with new library */
	lib_manager_update_sof_ctx((struct sof_man_fw_desc *)library_base_address, lib_id);

	return 0;
}

int lib_manager_load_library(uint32_t dma_id, uint32_t lib_id)
{
	struct lib_manager_dma_ext dma_ext;
	uint32_t addr_align;
	int ret;
	void *man_tmp_buffer;

	lib_manager_init();

	ret = lib_manager_dma_init(&dma_ext, dma_id);
	if (ret < 0)
		goto cleanup;
	ret = dma_get_attribute(dma_ext.dma, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (ret < 0)
		goto cleanup;

	/* allocate temporary manifest buffer */
	man_tmp_buffer = rballoc_align(0, SOF_MEM_CAPS_DMA, MAN_MAX_SIZE_V1_8, addr_align);
	if (!man_tmp_buffer) {
		ret = -ENOMEM;
		goto cleanup;
	}

	ret = lib_manager_dma_buffer_init(&dma_ext.dmabp, MAN_MAX_SIZE_V1_8, addr_align);
	if (ret < 0)
		goto cleanup;

	/* Load manifest to temporary buffer */
	ret = lib_manager_store_data(&dma_ext, man_tmp_buffer, MAN_MAX_SIZE_V1_8);
	if (ret < 0)
		goto cleanup;

	ret = lib_manager_store_library(&dma_ext, man_tmp_buffer, lib_id, addr_align);

cleanup:
	lib_manager_dma_buffer_free(&dma_ext.dmabp);

	if (man_tmp_buffer)
		rfree(man_tmp_buffer);

	lib_manager_dma_deinit(&dma_ext, dma_id);

	return ret;
}

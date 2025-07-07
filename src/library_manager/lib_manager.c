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

#include <rtos/clk.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/lib_manager.h>
#include <sof/llext_manager.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/modules.h>
#include <utilities/array.h>
#include <native_system_agent.h>
#include <api_version.h>
#include <module/module/api_ver.h>

#include <zephyr/cache.h>
#include <zephyr/drivers/mm/system_mm.h>
#if CONFIG_LLEXT
#include <zephyr/llext/llext.h>
#endif

#if CONFIG_LIBRARY_AUTH_SUPPORT
#include <auth/intel_auth_api.h>
#else
struct auth_api_ctx;
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(lib_manager, CONFIG_SOF_LOG_LEVEL);
/* 54cf5598-8b29-11ec-a8a3-0242ac120002 */

SOF_DEFINE_REG_UUID(lib_manager);

DECLARE_TR_CTX(lib_manager_tr, SOF_UUID(lib_manager_uuid), LOG_LEVEL_INFO);

struct lib_manager_dma_ext {
	struct sof_dma *dma;
	struct dma_chan_data *chan;
	uintptr_t dma_addr;		/**< buffer start pointer */
	uint32_t addr_align;
};

static struct ext_library loader_ext_lib;

#if CONFIG_LIBRARY_AUTH_SUPPORT
static int lib_manager_auth_init(struct auth_api_ctx *auth_ctx, void **auth_buffer)
{
	int ret;

	if (auth_api_version().major != AUTH_API_VERSION_MAJOR)
		return -EINVAL;

	*auth_buffer = rballoc_align(SOF_MEM_FLAG_KERNEL,
				     AUTH_SCRATCH_BUFF_SZ, CONFIG_MM_DRV_PAGE_SIZE);
	if (!*auth_buffer)
		return -ENOMEM;

	ret = auth_api_init(auth_ctx, *auth_buffer, AUTH_SCRATCH_BUFF_SZ, IMG_TYPE_LIB);
	if (ret != 0) {
		tr_err(&lib_manager_tr, "auth_api_init() failed with error: %d", ret);
		rfree(*auth_buffer);
		return -EACCES;
	}

	return 0;
}

static void lib_manager_auth_deinit(struct auth_api_ctx *auth_ctx, void *auth_buffer)
{
	ARG_UNUSED(auth_ctx);
	rfree(auth_buffer);
}

static int lib_manager_auth_proc(const void *buffer_data, size_t buffer_size,
				 enum auth_phase phase, struct auth_api_ctx *auth_ctx)
{
	int ret;

	ret = auth_api_init_auth_proc(auth_ctx, buffer_data, buffer_size, phase);

	if (ret != 0) {
		tr_err(&lib_manager_tr, "auth_api_init_auth_proc() failed with error: %d", ret);
		return -ENOTSUP;
	}

	/* The auth_api_busy() will timeouts internally in case of failure */
	while (auth_api_busy(auth_ctx))
		;

	ret = auth_api_result(auth_ctx);

	if (ret != AUTH_IMAGE_TRUSTED) {
		tr_err(&lib_manager_tr, "Untrusted library!");
		return -EACCES;
	}

	if (phase == AUTH_PHASE_LAST)
		auth_api_cleanup(auth_ctx);

	return 0;
}
#endif /* CONFIG_LIBRARY_AUTH_SUPPORT */

#if IS_ENABLED(CONFIG_MM_DRV)

#define PAGE_SZ		CONFIG_MM_DRV_PAGE_SIZE

static int lib_manager_load_data_from_storage(void __sparse_cache *vma, void *s_addr, uint32_t size,
					      uint32_t flags)
{
	/* Region must be first mapped as writable in order to initialize its contents. */
	int ret = sys_mm_drv_map_region((__sparse_force void *)vma, POINTER_TO_UINT(NULL), size,
					SYS_MM_MEM_PERM_RW);
	if (ret < 0)
		return ret;

	ret = memcpy_s((__sparse_force void *)vma, size, s_addr, size);
	if (ret < 0)
		return ret;

	dcache_writeback_region(vma, size);

	return sys_mm_drv_update_region_flags((__sparse_force void *)vma, size, flags);
}

static int lib_manager_load_module(const uint32_t module_id,
				   const struct sof_man_module *const mod)
{
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	const uintptr_t load_offset = POINTER_TO_UINT(ctx->base_addr);
	void *src;
	void __sparse_cache *va_base;
	size_t size;
	uint32_t flags;
	int ret, idx;

	for (idx = 0; idx < ARRAY_SIZE(mod->segment); ++idx) {
		if (!mod->segment[idx].flags.r.load)
			continue;

		flags = 0;

		if (mod->segment[idx].flags.r.code)
			flags = SYS_MM_MEM_PERM_EXEC;
		else if (!mod->segment[idx].flags.r.readonly)
			flags = SYS_MM_MEM_PERM_RW;

		src = UINT_TO_POINTER(mod->segment[idx].file_offset + load_offset);
		va_base = (void __sparse_cache *)UINT_TO_POINTER(mod->segment[idx].v_base_addr);
		size = mod->segment[idx].flags.r.length * PAGE_SZ;
		ret = lib_manager_load_data_from_storage(va_base, src, size, flags);
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	for (--idx; idx >= 0; --idx) {
		if (!mod->segment[idx].flags.r.load)
			continue;

		va_base = (void __sparse_cache *)UINT_TO_POINTER(mod->segment[idx].v_base_addr);
		size = mod->segment[idx].flags.r.length * PAGE_SZ;
		sys_mm_drv_unmap_region((__sparse_force void *)va_base, size);
	}

	return ret;
}

static int lib_manager_unload_module(const struct sof_man_module *const mod)
{
	void __sparse_cache *va_base;
	size_t size;
	uint32_t idx;
	int ret;

	for (idx = 0; idx < ARRAY_SIZE(mod->segment); ++idx) {
		if (!mod->segment[idx].flags.r.load)
			continue;

		va_base = (void __sparse_cache *)UINT_TO_POINTER(mod->segment[idx].v_base_addr);
		size = mod->segment[idx].flags.r.length * PAGE_SZ;
		ret = sys_mm_drv_unmap_region((__sparse_force void *)va_base, size);
		if (ret < 0)
			return ret;
	}

	return 0;
}

#ifdef CONFIG_LIBCODE_MODULE_SUPPORT
/* There are modules marked as lib_code. This is code shared between several modules inside
 * the library. Load all lib_code modules with first none lib_code module load.
 */
static int lib_manager_load_libcode_modules(const uint32_t module_id)
{
	const struct sof_man_fw_desc *const desc = lib_manager_get_library_manifest(module_id);
	struct ext_library *const ext_lib = ext_lib_get();
	const struct sof_man_module *module_entry = (struct sof_man_module *)
		((char *)desc + SOF_MAN_MODULE_OFFSET(0));
	const uint32_t lib_id = LIB_MANAGER_GET_LIB_ID(module_id);
	int ret, idx;

	if (++ext_lib->mods_exec_load_cnt > 1)
		return 0;

	for (idx = 0; idx < desc->header.num_module_entries; ++idx, ++module_entry) {
		if (module_entry->type.lib_code) {
			ret = lib_manager_load_module(lib_id << LIB_MANAGER_LIB_ID_SHIFT | idx,
						      module_entry);
			if (ret < 0)
				goto err;
		}
	}

	return 0;

err:
	for (--idx, --module_entry; idx >= 0; --idx, --module_entry) {
		if (module_entry->type.lib_code) {
			ret = lib_manager_unload_module(module_entry);
			if (ret < 0)
				goto err;
		}
	}

	return ret;
}

/* There are modules marked as lib_code. This is code shared between several modules inside
 * the library. Unload all lib_code modules with last none lib_code module unload.
 */
static int lib_manager_unload_libcode_modules(const uint32_t module_id)
{
	const struct sof_man_fw_desc *const desc = lib_manager_get_library_manifest(module_id);
	const struct sof_man_module *module_entry = (struct sof_man_module *)
		((char *)desc + SOF_MAN_MODULE_OFFSET(0));
	struct ext_library *const ext_lib = ext_lib_get();
	int ret, idx;

	if (--ext_lib->mods_exec_load_cnt > 0)
		return 0;

	for (idx = 0; idx < desc->header.num_module_entries; ++idx, ++module_entry) {
		if (module_entry->type.lib_code) {
			ret = lib_manager_unload_module(module_entry);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}
#endif /* CONFIG_LIBCODE_MODULE_SUPPORT */

static void lib_manager_get_instance_bss_address(uint32_t instance_id,
						 const struct sof_man_module *mod,
						 void __sparse_cache **va_addr, size_t *size)
{
	*size = mod->segment[SOF_MAN_SEGMENT_BSS].flags.r.length / mod->instance_max_count *
		PAGE_SZ;
	size_t inst_offset = *size * instance_id;
	*va_addr = (void __sparse_cache *)(mod->segment[SOF_MAN_SEGMENT_BSS].v_base_addr +
					   inst_offset);

	tr_dbg(&lib_manager_tr, "instance_bss_size: %#zx, pointer: %p", *size,
	       (__sparse_force void *)*va_addr);
}

static int lib_manager_allocate_module_instance(uint32_t instance_id, uint32_t is_pages,
						const struct sof_man_module *mod)
{
	size_t bss_size;
	void __sparse_cache *va_base;

	lib_manager_get_instance_bss_address(instance_id, mod, &va_base, &bss_size);

	if ((is_pages * PAGE_SZ) > bss_size) {
		tr_err(&lib_manager_tr, "invalid is_pages: %u, required: %u",
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

static int lib_manager_free_module_instance(uint32_t instance_id, const struct sof_man_module *mod)
{
	size_t bss_size;
	void __sparse_cache *va_base;

	lib_manager_get_instance_bss_address(instance_id, mod, &va_base, &bss_size);
	/*
	 * Unmap bss memory.
	 */
	return sys_mm_drv_unmap_region((__sparse_force void *)va_base, bss_size);
}

uintptr_t lib_manager_allocate_module(const struct comp_ipc_config *ipc_config,
				      const void *ipc_specific_config)
{
	const struct sof_man_module *mod;
	const struct ipc4_base_module_cfg *base_cfg = ipc_specific_config;
	const uint32_t module_id = IPC4_MOD_ID(ipc_config->id);
	int ret;

	tr_dbg(&lib_manager_tr, "mod_id: %#x", ipc_config->id);

	mod = lib_manager_get_module_manifest(module_id);
	if (!mod) {
		tr_err(&lib_manager_tr, "failed to get module descriptor");
		return 0;
	}

	if (module_is_llext(mod))
		return llext_manager_allocate_module(ipc_config, ipc_specific_config);

	ret = lib_manager_load_module(module_id, mod);
	if (ret < 0)
		return 0;

#ifdef CONFIG_LIBCODE_MODULE_SUPPORT
	ret = lib_manager_load_libcode_modules(module_id);
	if (ret < 0)
		goto err;
#endif /* CONFIG_LIBCODE_MODULE_SUPPORT */

	ret = lib_manager_allocate_module_instance(IPC4_INST_ID(ipc_config->id), base_cfg->is_pages,
						   mod);
	if (ret < 0) {
		tr_err(&lib_manager_tr, "module allocation failed: %d", ret);
#ifdef CONFIG_LIBCODE_MODULE_SUPPORT
		lib_manager_unload_libcode_modules(module_id);
#endif /* CONFIG_LIBCODE_MODULE_SUPPORT */
		goto err;
	}
	return mod->entry_point;

err:
	lib_manager_unload_module(mod);
	return 0;
}

int lib_manager_free_module(const uint32_t component_id)
{
	const struct sof_man_module *mod;
	const uint32_t module_id = IPC4_MOD_ID(component_id);
	int ret;

	tr_dbg(&lib_manager_tr, "mod_id: %#x", component_id);

	mod = lib_manager_get_module_manifest(module_id);
	if (!mod) {
		tr_err(&lib_manager_tr, "failed to get module descriptor");
		return -EINVAL;
	}

	if (module_is_llext(mod))
		return llext_manager_free_module(component_id);

	ret = lib_manager_unload_module(mod);
	if (ret < 0)
		return ret;

#ifdef CONFIG_LIBCODE_MODULE_SUPPORT
	ret = lib_manager_unload_libcode_modules(module_id);
	if (ret < 0)
		return ret;
#endif /* CONFIG_LIBCODE_MODULE_SUPPORT */

	ret = lib_manager_free_module_instance(IPC4_INST_ID(component_id), mod);
	if (ret < 0) {
		tr_err(&lib_manager_tr, "free module instance failed: %d", ret);
		return ret;
	}
	return 0;
}

#else /* CONFIG_MM_DRV */

#define PAGE_SZ		4096 /* equals to MAN_PAGE_SIZE used by rimage */

uintptr_t lib_manager_allocate_module(const struct comp_ipc_config *ipc_config,
				      const void *ipc_specific_config, const void **buildinfo)
{
	tr_err(&lib_manager_tr, "Dynamic module allocation is not supported");
	return 0;
}

int lib_manager_free_module(const uint32_t component_id)
{
	/* Since we cannot allocate the freeing is not considered to be an error */
	tr_warn(&lib_manager_tr, "Dynamic module freeing is not supported");
	return 0;
}
#endif /* CONFIG_MM_DRV */

void lib_manager_init(void)
{
	struct sof *sof = sof_get();

	if (!sof->ext_library)
		sof->ext_library = &loader_ext_lib;
}

const struct sof_man_fw_desc *lib_manager_get_library_manifest(int module_id)
{
	struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(module_id);
	uint8_t *buffptr = ctx ? ctx->base_addr : NULL;

	if (!buffptr)
		return NULL;
	return (struct sof_man_fw_desc *)(buffptr + SOF_MAN_ELF_TEXT_OFFSET);
}

static void lib_manager_update_sof_ctx(void *base_addr, uint32_t lib_id)
{
	struct ext_library *_ext_lib = ext_lib_get();
	/* Never freed, will panic if fails */
	struct lib_manager_mod_ctx *ctx = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
						  sizeof(*ctx));
	if (!ctx) {
		tr_err(&lib_manager_tr, "lib_manager_update_sof_ctx(): allocation failed");
		sof_panic(SOF_IPC_PANIC_IPC);
	}

	ctx->base_addr = base_addr;

	_ext_lib->desc[lib_id] = ctx;
	/* TODO: maybe need to call dcache_writeback here? */
}

const struct sof_man_module *lib_manager_get_module_manifest(const uint32_t module_id)
{
	const uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);
	const struct lib_manager_mod_ctx *const ctx = lib_manager_get_mod_ctx(module_id);
	const struct sof_man_fw_desc *desc;

	if (!ctx || !ctx->base_addr)
		return NULL;

	desc = (const struct sof_man_fw_desc *)((const char *)ctx->base_addr +
							      SOF_MAN_ELF_TEXT_OFFSET);

	if (entry_index >= desc->header.num_module_entries) {
		tr_err(&lib_manager_tr, "Entry index %d out of bounds.", entry_index);
		return NULL;
	}

	return (const struct sof_man_module *)((const char *)desc +
					       SOF_MAN_MODULE_OFFSET(entry_index));
}

/*
 * \brief Load module code, allocate its instance and create a module adapter component.
 * \param[in] drv - component driver pointer.
 * \param[in] config - component ipc descriptor pointer.
 * \param[in] spec - passdowned data from driver.
 *
 * \return: a pointer to newly created module adapter component on success. NULL on error.
 */
static struct comp_dev *lib_manager_module_create(const struct comp_driver *drv,
						  const struct comp_ipc_config *config,
						  const void *spec)
{
	const struct ipc_config_process *args = (struct ipc_config_process *)spec;
	const uint32_t module_id = IPC4_MOD_ID(config->id);
	const uint32_t instance_id = IPC4_INST_ID(config->id);
	const uint32_t log_handle = (uint32_t)drv->tctx;
	byte_array_t mod_cfg;

	/*
	 * Variable used by llext_manager to temporary store llext handle before creation
	 * a instance of processing_module.
	 */
	struct comp_dev *dev;

	/* At this point module resources are allocated and it is moved to L2 memory. */
	const uint32_t module_entry_point = lib_manager_allocate_module(config,
									args->data);

	if (!module_entry_point) {
		tr_err(&lib_manager_tr, "lib_manager_allocate_module() failed!");
		return NULL;
	}
	tr_dbg(&lib_manager_tr, "start");

	mod_cfg.data = (uint8_t *)args->data;
	/* Intel modules expects DW size here */
	mod_cfg.size = args->size >> 2;

	((struct comp_driver *)drv)->adapter_ops = native_system_agent_start(module_entry_point,
									     module_id,	instance_id,
									     0, log_handle,
									     &mod_cfg);

	if (!drv->adapter_ops) {
		lib_manager_free_module(module_id);
		tr_err(&lib_manager_tr, "native_system_agent_start failed!");
		return NULL;
	}

	dev = module_adapter_new(drv, config, spec);
	if (!dev)
		lib_manager_free_module(module_id);

	return dev;
}

static void lib_manager_module_free(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	const struct comp_ipc_config *const config = &mod->dev->ipc_config;
	int ret;

	/* This call invalidates dev, mod and config pointers! */
	module_adapter_free(dev);

	/* Free module resources allocated in L2 memory. */
	ret = lib_manager_free_module(config->id);
	if (ret < 0)
		comp_err(dev, "lib_manager_free_module() failed!");
}

static void lib_manager_prepare_module_adapter(struct comp_driver *drv, const struct sof_uuid *uuid)
{
	drv->type = SOF_COMP_MODULE_ADAPTER;
	drv->uid = uuid;
	drv->tctx = &lib_manager_tr;
	drv->ops.create = lib_manager_module_create;
	drv->ops.prepare = module_adapter_prepare;
	drv->ops.params = module_adapter_params;
	drv->ops.copy = module_adapter_copy;
#if CONFIG_IPC_MAJOR_3
	drv->ops.cmd = module_adapter_cmd;
#endif
	drv->ops.trigger = module_adapter_trigger;
	drv->ops.reset = module_adapter_reset;
	drv->ops.free = lib_manager_module_free;
	drv->ops.set_large_config = module_set_large_config;
	drv->ops.get_large_config = module_get_large_config;
	drv->ops.get_attribute = module_adapter_get_attribute;
	drv->ops.set_attribute = module_adapter_set_attribute;
	drv->ops.bind = module_adapter_bind;
	drv->ops.unbind = module_adapter_unbind;
	drv->ops.get_total_data_processed = module_adapter_get_total_data_processed;
	drv->ops.dai_get_hw_params = module_adapter_get_hw_params;
	drv->ops.position = module_adapter_position;
	drv->ops.dai_ts_config = module_adapter_ts_config_op;
	drv->ops.dai_ts_start = module_adapter_ts_start_op;
	drv->ops.dai_ts_stop = module_adapter_ts_stop_op;
	drv->ops.dai_ts_get = module_adapter_ts_get_op;
#if CONFIG_INTEL_MODULES
	drv->adapter_ops = &processing_module_adapter_interface;
#endif
}

int lib_manager_register_module(const uint32_t component_id)
{
	const struct sof_man_fw_desc *const desc = lib_manager_get_library_manifest(component_id);
	const uint32_t entry_index = LIB_MANAGER_GET_MODULE_INDEX(component_id);
	const struct sof_module_api_build_info *build_info;
	struct comp_driver_info *new_drv_info;
	struct comp_driver *drv = NULL;
	const struct sof_man_module *mod;
	int ret;

	if (!desc) {
		tr_err(&lib_manager_tr, "Error: Couldn't find loadable module with id %u.",
		       component_id);
		return -ENOENT;
	}

	if (entry_index >= desc->header.num_module_entries) {
		tr_err(&lib_manager_tr, "Entry index %u out of bounds.", entry_index);
		return -ENOENT;
	}

	/* allocate new comp_driver_info */
	new_drv_info = rmalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			       sizeof(struct comp_driver_info));

	if (!new_drv_info) {
		tr_err(&lib_manager_tr, "failed to allocate comp_driver_info");
		ret = -ENOMEM;
		goto cleanup;
	}

	drv = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
		      sizeof(struct comp_driver));
	if (!drv) {
		tr_err(&lib_manager_tr, "failed to allocate comp_driver");
		ret = -ENOMEM;
		goto cleanup;
	}

	mod = (const struct sof_man_module *)((const uint8_t *)desc +
					      SOF_MAN_MODULE_OFFSET(entry_index));
	const struct sof_uuid *uid = (const struct sof_uuid *)&mod->uuid;

	lib_manager_prepare_module_adapter(drv, uid);

	/*
	 * llext modules store build info structure in separate section which is not accessible now.
	 */
	if (!module_is_llext(mod)) {
		build_info = (const struct sof_module_api_build_info *)((const char *)desc -
			     SOF_MAN_ELF_TEXT_OFFSET +
			     mod->segment[SOF_MAN_SEGMENT_TEXT].file_offset);

		tr_info(&lib_manager_tr, "Module API version: %u.%u.%u, format: 0x%x",
			build_info->api_version_number.fields.major,
			build_info->api_version_number.fields.middle,
			build_info->api_version_number.fields.minor,
			build_info->format);

		/* Check if module is IADK */
		if (IS_ENABLED(CONFIG_INTEL_MODULES) &&
		    build_info->format == IADK_MODULE_API_BUILD_INFO_FORMAT &&
		    build_info->api_version_number.full == IADK_MODULE_API_CURRENT_VERSION) {
			/* Use module_adapter functions */
			drv->ops.create = module_adapter_new;
		} else {
			/* Check if module is NOT native */
			if (build_info->format != SOF_MODULE_API_BUILD_INFO_FORMAT ||
			    build_info->api_version_number.full != SOF_MODULE_API_CURRENT_VERSION) {
				tr_err(&lib_manager_tr, "Unsupported module API version");
				ret = -ENOEXEC;
				goto cleanup;
			}
		}
	}

	/* Fill the new_drv_info structure with already known parameters */
	new_drv_info->drv = drv;

	/* Register new driver in the list */
	ret = comp_register(new_drv_info);

cleanup:
	if (ret < 0) {
		rfree(drv);
		rfree(new_drv_info);
	}

	return ret;
}

static int lib_manager_dma_buffer_alloc(struct lib_manager_dma_ext *dma_ext,
					uint32_t size)
{
	/*
	 * allocate new buffer: this is the actual DMA buffer but we
	 * traditionally allocate a cached address for it
	 */
	dma_ext->dma_addr = (uintptr_t)rballoc_align(SOF_MEM_FLAG_COHERENT | SOF_MEM_FLAG_DMA, size,
						     dma_ext->addr_align);
	if (!dma_ext->dma_addr) {
		tr_err(&lib_manager_tr, "alloc failed");
		return -ENOMEM;
	}

	tr_dbg(&lib_manager_tr, "address: %#lx, size: %u", dma_ext->dma_addr, size);

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
	dma_ext->dma = sof_dma_get(SOF_DMA_DIR_HMEM_TO_LMEM, 0, SOF_DMA_DEV_HOST,
				   SOF_DMA_ACCESS_EXCLUSIVE);
	if (!dma_ext->dma) {
		tr_err(&lib_manager_tr, "dma_ext->dma = NULL");
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

		sof_dma_put(dma_ext->dma);
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

	tr_err(&lib_manager_tr, "timeout during DMA transfer");
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
	uint32_t flags = SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_L3 | SOF_MEM_FLAG_DMA;
#else
	uint32_t flags = SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_DMA;
#endif

	uint32_t addr_align = PAGE_SZ;
	/* allocate new buffer: cached alias */
	local_add = (__sparse_force void __sparse_cache *)rballoc_align(flags, size, addr_align);

	if (!local_add) {
		tr_err(&lib_manager_tr, "alloc failed");
		return NULL;
	}

	return local_add;
}

static int lib_manager_store_library(struct lib_manager_dma_ext *dma_ext,
				     const void __sparse_cache *man_buffer,
				     uint32_t lib_id, struct auth_api_ctx *auth_ctx)
{
	void __sparse_cache *library_base_address;
	const struct sof_man_fw_desc *man_desc = (struct sof_man_fw_desc *)
		((__sparse_force uint8_t *)man_buffer + SOF_MAN_ELF_TEXT_OFFSET);
	uint32_t preload_size = man_desc->header.preload_page_count * PAGE_SZ;
	int ret;

	/*
	 * The module manifest structure always has its maximum size regardless of
	 * the actual size of the manifest.
	 */
	if (preload_size < MAN_MAX_SIZE_V1_8) {
		tr_err(&lib_manager_tr, "Invalid preload_size value %x.", preload_size);
		return -EINVAL;
	}

	/* Prepare storage memory, note: it is never freed, library unloading is unsupported */
	/*
	 * Prepare storage memory, note: it is never freed, it is assumed, that this
	 * memory is abundant, so we store all loaded modules there permanently
	 */
	library_base_address = lib_manager_allocate_store_mem(preload_size, 0);
	if (!library_base_address)
		return -ENOMEM;

	tr_dbg(&lib_manager_tr, "pointer: %p", (__sparse_force void *)library_base_address);

#if CONFIG_LIBRARY_AUTH_SUPPORT
	/* AUTH_PHASE_FIRST - checks library manifest only. */
	ret = lib_manager_auth_proc((__sparse_force const void *)man_buffer,
				    MAN_MAX_SIZE_V1_8, AUTH_PHASE_FIRST, auth_ctx);
	if (ret < 0) {
		rfree((__sparse_force void *)library_base_address);
		return ret;
	}
#endif /* CONFIG_LIBRARY_AUTH_SUPPORT */

	/* Copy data from temp_mft_buf to destination memory (pointed by library_base_address) */
	memcpy_s((__sparse_force void *)library_base_address, MAN_MAX_SIZE_V1_8,
		 (__sparse_force const void *)man_buffer, MAN_MAX_SIZE_V1_8);

	/* Copy remaining library part into storage buffer */
	ret = lib_manager_store_data(dma_ext, (uint8_t __sparse_cache *)library_base_address +
				     MAN_MAX_SIZE_V1_8, preload_size - MAN_MAX_SIZE_V1_8);
	if (ret < 0) {
		rfree((__sparse_force void *)library_base_address);
		return ret;
	}

#if CONFIG_LIBRARY_AUTH_SUPPORT
	/* AUTH_PHASE_LAST - do final library authentication checks */
	ret = lib_manager_auth_proc((__sparse_force void *)library_base_address,
				    preload_size - MAN_MAX_SIZE_V1_8, AUTH_PHASE_LAST, auth_ctx);
	if (ret < 0) {
		rfree((__sparse_force void *)library_base_address);
		return ret;
	}
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

	dma_ext = rzalloc(SOF_MEM_FLAG_KERNEL,
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

#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	/*
	 * make sure that the DSP is running full speed for the duration of
	 * library loading
	 */
	ret = core_kcps_adjust(cpu_get_id(), CLK_MAX_CPU_HZ / 1000);
	if (ret < 0)
		goto err_dma_buffer;
#endif

	ret = dma_config(dma_ext->chan->dma->z_dev, dma_ext->chan->index, &config);
	if (ret < 0)
		goto err_dma;

	ret = dma_start(dma_ext->chan->dma->z_dev, dma_ext->chan->index);
	if (ret < 0)
		goto err_dma;

	_ext_lib->runtime_data = dma_ext;

	return 0;

err_dma:
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	core_kcps_adjust(cpu_get_id(), -(CLK_MAX_CPU_HZ / 1000));
#endif

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
		tr_err(&lib_manager_tr, "invalid lib_id: %u", lib_id);
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
			rballoc_align(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_DMA,
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
	struct auth_api_ctx auth_ctx;
	void *auth_buffer;

	/* Initialize authentication support */
	ret = lib_manager_auth_init(&auth_ctx, &auth_buffer);
	if (ret < 0)
		goto stop_dma;

	ret = lib_manager_store_library(dma_ext, man_tmp_buffer, lib_id, &auth_ctx);

	lib_manager_auth_deinit(&auth_ctx, auth_buffer);
#else
	ret = lib_manager_store_library(dma_ext, man_tmp_buffer, lib_id, NULL);
#endif /* CONFIG_LIBRARY_AUTH_SUPPORT */

stop_dma:
	ret2 = dma_stop(dma_ext->chan->dma->z_dev, dma_ext->chan->index);
	if (ret2 < 0) {
		tr_err(&lib_manager_tr, "error stopping DMA: %d", ret);
		if (!ret)
			ret = ret2;
	}

	rfree((__sparse_force void *)man_tmp_buffer);

cleanup:
	rfree((void *)dma_ext->dma_addr);
	lib_manager_dma_deinit(dma_ext, dma_id);
	rfree(dma_ext);
	_ext_lib->runtime_data = NULL;

	uint32_t module_id = lib_id << LIB_MANAGER_LIB_ID_SHIFT;
	const struct sof_man_module *mod = lib_manager_get_module_manifest(module_id);

	if (!ret && module_is_llext(mod))
		/* Auxiliary LLEXT libraries need to be linked upon loading */
		ret = llext_manager_add_library(module_id);

#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	core_kcps_adjust(cpu_get_id(), -(CLK_MAX_CPU_HZ / 1000));
#endif

	if (!ret)
		tr_info(&lib_manager_tr, "loaded library id: %u", lib_id);

	return ret;
}

// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/ring_buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <module/module/base.h>
#include <sof/common.h>
#include <rtos/interrupt.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <ipc/dai.h>
#include <ipc4/handler.h>
#include <sof/ipc/msg.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/vregion.h>
#include <sof/list.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/symbol.h>
#include <rtos/wait.h>

/* TODO: Remove platform-specific code, see https://github.com/thesofproject/sof/issues/7549 */
#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_ACE) || defined(CONFIG_INTEL_ADSP_CAVS)
#define RIMAGE_MANIFEST 1
#endif

#ifdef RIMAGE_MANIFEST
#include <adsp_memory.h>
#endif

#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <rimage/cavs/cavs_ext_manifest.h>
#include <rimage/sof/user/manifest.h>
#include <ipc4/base-config.h>
#include <ipc/header.h>
#include <ipc4/notification.h>
#include <ipc4/pipeline.h>
#include <ipc4/module.h>
#include <ipc4/error_status.h>
#include <sof/lib_manager.h>
#include <sof/tlv.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sof/debug/telemetry/telemetry.h>
#include <sof/debug/telemetry/performance_monitor.h>

/* Kernel part of the IPC4 helpers: the component driver lookup, which reads the
 * IMR manifest and the driver list, module creation in kernel mode, IDC to other
 * cores and the chain DMA manager. The part that also runs in user mode is in
 * src/ipc/ipc4/helper.c
 */

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx comp_tr;

static const struct comp_driver *ipc4_get_drv(const void *uuid);

#if CONFIG_LIBRARY
static const struct comp_driver *ipc4_library_get_comp_drv(unsigned char *data)
{
	return ipc4_get_drv(data);
}
#endif /* CONFIG_LIBRARY */

/* Only called from ipc4_init_module_instance(), which is __cold */
__cold struct comp_dev *comp_new_ipc4(const struct ipc4_module_init_instance *module_init)
{
	struct comp_ipc_config ipc_config;
	const struct comp_driver *drv;
	struct comp_dev *dev;
	unsigned char *data;
	uint32_t comp_id = IPC4_COMP_ID(module_init->primary.r.module_id,
					module_init->primary.r.instance_id);

	assert_can_be_cold();

	int ret = ipc4_comp_new_config(&ipc_config, module_init);

	if (ret < 0)
		return NULL;

#ifdef CONFIG_DCACHE_LINE_SIZE
	if (!IS_ENABLED(CONFIG_LIBRARY))
		sys_cache_data_invd_range((__sparse_force void __sparse_cache *)
					  MAILBOX_HOSTBOX_BASE,
					  ALIGN_UP(ipc_config.ipc_config_size,
						   CONFIG_DCACHE_LINE_SIZE));
#endif
	data = ipc4_get_comp_new_data();
#if CONFIG_LIBRARY
	ipc_config.ipc_config_size -= sizeof(struct sof_uuid);
	drv = ipc4_library_get_comp_drv(data + ipc_config.ipc_config_size);
#else
	drv = ipc4_get_comp_drv(IPC4_MOD_ID(comp_id));
#endif
	if (!drv)
		return NULL;

	if (!drv->ops.create) {
		tr_err(&ipc_tr, "comp 0x%x: module %u is not instantiable", comp_id,
		       (uint32_t)IPC4_MOD_ID(comp_id));
		return NULL;
	}

	if (drv->type == SOF_COMP_MODULE_ADAPTER) {
		const struct ipc_config_process spec = {
			.data = data,
			.size = ipc_config.ipc_config_size,
		};

		dev = drv->ops.create(drv, &ipc_config, &spec);
	} else {
		dev = drv->ops.create(drv, &ipc_config, data);
	}
	if (!dev)
		return NULL;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
	/* init global performance measurement */
	dev->perf_data.perf_data_item = perf_data_getnext();
	/* this can be null, just no performance measurements in this case */
	if (dev->perf_data.perf_data_item) {
		dev->perf_data.perf_data_item->item.resource_id = comp_id;
		if (perf_meas_get_state() != IPC4_PERF_MEASUREMENTS_DISABLED)
			comp_init_performance_data(dev);
	}
#endif

	ipc4_add_comp_dev(dev);

	comp_update_ibs_obs_cpc(dev);

	return dev;
}

#ifdef CONFIG_SOF_USERSPACE_LL
int ipc4_user_module_load(const struct comp_driver *drv,
			  const struct ipc4_module_init_instance *mi)
{
	/*
	 * A large part of module instantiation is executed in the userspace
	 * mode, including calling the module's .init() method. But another part
	 * of it has to be executed in the kernel mode, including calling
	 * lib_manager_mod_create_priv(). This is the privileged part of module
	 * instantiation.
	 */
	struct comp_ipc_config ipc_config;
	int ret = ipc4_comp_new_config(&ipc_config, mi);

	if (ret < 0)
		return ret;

	const struct ipc_config_process spec = {
		.data = ipc4_get_comp_new_data(),
		.size = ipc_config.ipc_config_size,
	};

#if CONFIG_DCACHE_LINE_SIZE && !CONFIG_LIBRARY
	sys_cache_data_invd_range((__sparse_force void __sparse_cache *)spec.data, spec.size);
#endif

	struct userspace_context *userspace = NULL;

	return lib_manager_mod_create_priv(drv, &ipc_config, &spec, NULL, &userspace);
}
#endif /* CONFIG_SOF_USERSPACE_LL */

__cold int ipc_comp_free_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_FREE, IDC_MSG_FREE_EXT(dev->ipc_config.id),
		dev->ipc_config.core,};

	assert_can_be_cold();

	return idc_send_msg(&msg, IDC_BLOCKING);
}

#if CONFIG_COMP_CHAIN_DMA
/* Only called from ipc4_process_chain_dma(), which is __cold */
__cold int ipc4_chain_manager_create(const struct ipc4_chain_dma *cdma)
{
	const struct sof_uuid uuid = SOF_REG_UUID(chain_dma);
	const struct comp_driver *drv;
	struct comp_dev *dev;

	assert_can_be_cold();

	drv = ipc4_get_drv(&uuid);
	if (!drv)
		return -EINVAL;

	dev = drv->ops.create(drv, NULL, cdma);
	if (!dev)
		return -EINVAL;

	/* differentiate instance by unique ids assignment */
	const uint32_t comp_id = IPC4_COMP_ID(cdma->primary.r.host_dma_id
					      + IPC4_MAX_MODULE_COUNT, 0);
	dev->ipc_config.id = comp_id;
	dev->ipc_config.pipeline_id = cdma->primary.r.host_dma_id + IPC4_MAX_MODULE_COUNT;

	return ipc4_add_comp_dev(dev);
}

/* Only called from ipc4_process_chain_dma(), which is __cold */
__cold int ipc4_chain_dma_state(struct comp_dev *dev, const struct ipc4_chain_dma *cdma)
{
	const bool allocate = cdma->primary.r.allocate;
	const bool enable = cdma->primary.r.enable;
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	struct list_item *clist, *_tmp;
	int ret;

	assert_can_be_cold();

	if (!dev)
		return -EINVAL;

	if (allocate) {
		if (enable)
			ret = comp_trigger(dev, COMP_TRIGGER_START);
		else
			ret = comp_trigger(dev, COMP_TRIGGER_PAUSE);
	} else {
		if (enable)
			return -EINVAL;

		/* remove chain part */
		ret = comp_trigger(dev, COMP_TRIGGER_PAUSE);
		if (ret < 0)
			return ret;

		list_for_item_safe(clist, _tmp, &ipc->comp_list) {
			icd = container_of(clist, struct ipc_comp_dev, list);
			if (icd->cd != dev)
				continue;
			list_item_del(&icd->list);
			sof_heap_free(sof_sys_user_heap_get(), icd);
			break;
		}
		comp_free(dev);
	}
	return ret;
}
#endif /* CONFIG_COMP_CHAIN_DMA */

int ipc4_process_on_core(uint32_t core, bool blocking)
{
	int ret;

	ret = ipc_process_on_core(core, blocking);
	switch (ret) {
	case 0:
	case 1:
		return IPC4_SUCCESS;
	case -EACCES:
		return IPC4_INVALID_CORE_ID;
	case -ETIME:
	case -EBUSY:
		return IPC4_BUSY;
	default:
		return IPC4_FAILURE;
	}

	return IPC4_SUCCESS;
}

__cold static const struct comp_driver *ipc4_search_for_drv(const void *uuid)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;
	const struct comp_driver *drv = NULL;
	struct comp_driver_info *info;

	assert_can_be_cold();

	/* search driver list with UUID */
	list_for_item(clist, &drivers->list) {
		info = container_of(clist, struct comp_driver_info, list);
		if (!memcmp(info->drv->uid, uuid, UUID_SIZE)) {
			tr_dbg(&comp_tr, "found type %d, uuid %pU",
			       info->drv->type, SOF_DRV_UID_NAME(info->drv));
			drv = info->drv;
			break;
		}
	}

	return drv;
}

__cold static const struct comp_driver *ipc4_get_drv(const void *uuid)
{
	const struct sof_uuid *const __maybe_unused sof_uuid = (const struct sof_uuid *)uuid;
	const struct comp_driver *drv;

	assert_can_be_cold();

	drv = ipc4_search_for_drv(uuid);
	if (!drv)
		tr_err(&comp_tr,
		       "the provided UUID (%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x) can't be found!",
		       sof_uuid->a, sof_uuid->b, sof_uuid->c, sof_uuid->d[0], sof_uuid->d[1],
		       sof_uuid->d[2], sof_uuid->d[3], sof_uuid->d[4], sof_uuid->d[5],
		       sof_uuid->d[6], sof_uuid->d[7]);

	return drv;
}

#if defined(CONFIG_ARCH_POSIX_LIBFUZZER) && !defined(RIMAGE_MANIFEST)
/*
 * ipc4_get_fuzzer_drv - resolve a module driver for native_sim fuzz builds.
 *
 * native_sim fuzz builds have no rimage manifest, so the manifest lookup in
 * ipc4_get_comp_drv() can never resolve a driver and every module instance
 * verb (INIT_INSTANCE, CONFIG_GET/SET, LARGE_CONFIG, BIND, UNBIND,
 * DELETE_INSTANCE) would be unreachable from the fuzzer.
 *
 * Mirror the IPC3 whitebox hack in posix/ipc.c and treat module_id as a
 * 1-based index into the runtime component driver list, counting only
 * instantiable module-adapter drivers so the fuzzer's module_id space matches
 * what a real signed manifest would expose.
 */
static const struct comp_driver *ipc4_get_fuzzer_drv(uint32_t module_id)
{
	static const struct sof_uuid basefw_reg_uuid = SOF_REG_UUID(basefw);
	struct comp_driver_list *dlist = comp_drivers_get();
	struct list_item *iter;
	uint32_t idx = 0;

	if (!module_id)
		return ipc4_search_for_drv(&basefw_reg_uuid);

	list_for_item(iter, &dlist->list) {
		struct comp_driver_info *inf =
			container_of(iter, struct comp_driver_info, list);

		/*
		 * Skip query-only entries (e.g. BaseFW) that cannot be
		 * instantiated because they have no create op.
		 */
		if (!inf->drv->ops.create)
			continue;

		/*
		 * A real signed manifest only lists module-adapter modules.
		 * The internal gateway drivers (SOF_COMP_HOST, SOF_COMP_DAI)
		 * are registered for IPC3 but are never IPC4 modules: on the
		 * IPC4 path they receive no params() pass and cannot be
		 * configured (e.g. their DMA buffer is never allocated). Skip
		 * non-module-adapter drivers so the fuzzer's module_id space
		 * matches a real manifest and cannot map to an unconfigurable
		 * component.
		 */
		if (inf->drv->type != SOF_COMP_MODULE_ADAPTER)
			continue;

		if (++idx == module_id)
			return inf->drv;
	}

	tr_err(&comp_tr, "no instantiable driver at module_id %u (%u available)",
	       module_id, idx);
	return NULL;
}
#endif /* defined(CONFIG_ARCH_POSIX_LIBFUZZER) && !defined(RIMAGE_MANIFEST) */

/*
 * Called from
 * - ipc4_get_large_config_module_instance()
 * - ipc4_set_large_config_module_instance()
 * - comp_new_ipc4()
 * all of which are __cold
 */
__cold const struct comp_driver *ipc4_get_comp_drv(uint32_t module_id)
{
	const struct sof_man_fw_desc *desc = NULL;
	const struct sof_man_module *mod;
	uint32_t entry_index;

	assert_can_be_cold();

#ifdef RIMAGE_MANIFEST
	desc = (const struct sof_man_fw_desc *)IMR_BOOT_LDR_MANIFEST_BASE;
#elif defined(CONFIG_ARCH_POSIX_LIBFUZZER)
	return ipc4_get_fuzzer_drv(module_id);
#else
	/* Non-rimage platforms have no component facility yet.
	 * This needs to move to the platform layer.
	 */
	return NULL;
#endif

	uint32_t lib_idx = LIB_MANAGER_GET_LIB_ID(module_id);

	if (lib_idx == 0) {
		/* module_id 0 is used for base fw which is in entry 1 or 2 */
		if (!module_id)
			entry_index = 1 + IS_ENABLED(CONFIG_COLD_STORE_EXECUTE_DRAM);
		else
			entry_index = module_id;

		if (entry_index >= desc->header.num_module_entries) {
			tr_err(&comp_tr, "Error: entry index %d out of bounds.", entry_index);
			return NULL;
		}

		mod = (const struct sof_man_module *)((const char *)desc +
						      SOF_MAN_MODULE_OFFSET(entry_index));
	} else {
		/* Library index greater than 0 possible only when LIBRARY_MANAGER
		 * support enabled.
		 */
#if CONFIG_LIBRARY_MANAGER
		mod = lib_manager_get_module_manifest(module_id);

		if (!mod) {
			tr_err(&comp_tr, "Error: Couldn't find loadable module with id %#x.",
			       module_id);
			return NULL;
		}
#else
		tr_err(&comp_tr, "Error: lib index:%d, while loadable libraries are not supported!!!",
		       lib_idx);
		return NULL;
#endif
	}

#if CONFIG_LIBRARY_MANAGER
	/* Check already registered components */
	const struct comp_driver *drv = ipc4_search_for_drv(&mod->uuid);

	if (drv)
		return drv;

	/* New module not registered yet. */
	lib_manager_register_module(module_id);
#endif
	return ipc4_get_drv(&mod->uuid);
}

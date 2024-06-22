// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@linux.intel.com>
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/modules.h>
#include <utilities/array.h>
#include <iadk_module_adapter.h>
#include <system_agent.h>
#include <sof/lib_manager.h>
#include <sof/audio/module_adapter/module/module_interface.h>

/* Intel module adapter is an extension to SOF module adapter component that allows to integrate
 * modules developed under IADK (Intel Audio Development Kit) Framework. IADK modules uses uniform
 * set of interfaces and are linked into separate library. These modules are loaded in runtime
 * through library_manager and then after registration into SOF component infrastructure are
 * interfaced through module adapter API.
 *
 * There is variety of modules developed under IADK Framework by 3rd party vendors. The assumption
 * here is to integrate these modules with SOF infrastructure without modules code modifications.
 * Another assumption is that the 3rd party modules should be loaded in runtime without need
 * of rebuild the base firmware.
 * Therefore C++ function, structures and variables definition are here kept with original form from
 * IADK Framework. This provides binary compatibility for already developed 3rd party modules.
 *
 * Since IADK modules uses ProcessingModuleInterface to control/data transfer and AdspSystemService
 * to use base FW services from internal module code, there is a communication shim layer defined
 * in intel directory.
 *
 * Since ProcessingModuleInterface consists of virtual functions, there are C++ -> C iadk_wrappers
 * defined to access the interface calls from SOF code.
 *
 * There are three entities in intel module adapter package:
 *  - System Agent - A mediator to allow the custom module to interact with the base SOF FW.
 *                   It calls IADK module entry point and provides all necessary information to
 *                   connect both sides of ProcessingModuleInterface and System Service.
 *  - System Service - exposes of SOF base FW services to the module.
 *  - Processing Module Adapter - SOF base FW side of ProcessingModuleInterface API
 */

LOG_MODULE_REGISTER(sof_modules, CONFIG_SOF_LOG_LEVEL);
/* ee2585f2-e7d8-43dc-90ab-4224e00c3e84 */
SOF_DEFINE_UUID("modules", modules_uuid, 0xee2585f2, 0xe7d8, 0x43dc,
		    0x90, 0xab, 0x42, 0x24, 0xe0, 0x0c, 0x3e, 0x84);
DECLARE_TR_CTX(intel_codec_tr, SOF_UUID(modules_uuid), LOG_LEVEL_INFO);

/**
 * \brief modules_init.
 * \param[in] mod - processing module pointer.
 *
 * \return: zero on success
 *          error code on failure
 */
static int modules_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	const struct comp_driver *const drv = dev->drv;
	const struct ipc4_base_module_cfg *src_cfg = &md->cfg.base_cfg;
	const struct comp_ipc_config *config = &dev->ipc_config;
	void *system_agent;

	uintptr_t module_entry_point = lib_manager_allocate_module(mod, config, src_cfg);

	if (module_entry_point == 0) {
		comp_err(dev, "modules_init(), lib_manager_allocate_module() failed!");
		return -EINVAL;
	}

	/* At this point module resources are allocated and it is moved to L2 memory. */
	comp_info(dev, "modules_init() start");

	const uint32_t module_id = IPC4_MOD_ID(config->id);
	const uint32_t instance_id = IPC4_INST_ID(config->id);
	const uint32_t log_handle = (uint32_t)drv->tctx;

	byte_array_t mod_cfg = {
		.data = (uint8_t *)md->cfg.init_data,
		/* Intel modules expects DW size here */
		.size = md->cfg.size >> 2,
	};

	system_agent = system_agent_start(module_entry_point, module_id, instance_id, 0, log_handle,
					  &mod_cfg);

	module_set_private_data(mod, system_agent);

	md->mpd.in_buff_size = src_cfg->ibs;
	md->mpd.out_buff_size = src_cfg->obs;

	mod->proc_type = MODULE_PROCESS_TYPE_SOURCE_SINK;
	return iadk_wrapper_init(system_agent);
}

/**
 * \brief modules_prepare.
 * \param[in] mod - processing module pointer.
 *
 * \return: zero on success
 *          error code on failure
 *
 * \note:   We use ipc4_base_module_cfg since this is only what we know about module
 *          configuration. Its internal structure is proprietary to the module implementation.
 *          There is one assumption - all IADK modules utilize IPC4 protocol.
 */
static int modules_prepare(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "modules_prepare()");

	return iadk_wrapper_prepare(module_get_private_data(mod));
}

static int modules_process(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	return iadk_wrapper_process(module_get_private_data(mod), sources,
				    num_of_sources, sinks, num_of_sinks);
}

/**
 * \brief modules_free.
 * \param[in] mod - processing module pointer.
 *
 * \return: zero on success
 *          error code on failure
 */
static int modules_free(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_info(dev, "modules_free()");
	ret = iadk_wrapper_free(module_get_private_data(mod));
	if (ret)
		comp_err(dev, "modules_free(): iadk_wrapper_free failed with error: %d", ret);

	/* Free module resources allocated in L2 memory. */
	ret = lib_manager_free_module(dev->ipc_config.id);
	if (ret < 0)
		comp_err(dev, "modules_free(), lib_manager_free_module() failed!");

	return ret;
}

/*
 * \brief modules_set_configuration - Common method to assemble large configuration message
 * \param[in] mod - struct processing_module pointer
 * \param[in] config_id - Configuration ID
 * \param[in] pos - position of the fragment in the large message
 * \param[in] data_offset_size: size of the whole configuration if it is the first fragment or the
 *	      only fragment. Otherwise, it is the offset of the fragment in the whole
 *	      configuration.
 * \param[in] fragment: configuration fragment buffer
 * \param[in] fragment_size: size of @fragment
 * \params[in] response: optional response buffer to fill
 * \params[in] response_size: size of @response
 *
 * \return: 0 upon success or error upon failure
 */
static int modules_set_configuration(struct processing_module *mod, uint32_t config_id,
				     enum module_cfg_fragment_position pos,
				     uint32_t data_offset_size, const uint8_t *fragment,
				     size_t fragment_size, uint8_t *response,
				     size_t response_size)
{
	return iadk_wrapper_set_configuration(module_get_private_data(mod), config_id, pos,
					      data_offset_size, fragment, fragment_size,
					      response, response_size);
}

/*
 * \brief modules_get_configuration - Common method to retrieve module configuration
 * \param[in] mod - struct processing_module pointer
 * \param[in] config_id - Configuration ID
 * \param[in] pos - position of the fragment in the large message
 * \param[in] data_offset_size: size of the whole configuration if it is the first fragment or the
 *	      only fragment. Otherwise, it is the offset of the fragment in the whole configuration.
 * \param[in] fragment: configuration fragment buffer
 * \param[in] fragment_size: size of @fragment
 *
 * \return: 0 upon success or error upon failure
 */
static int modules_get_configuration(struct processing_module *mod, uint32_t config_id,
				     uint32_t *data_offset_size, uint8_t *fragment,
				     size_t fragment_size)
{
	return iadk_wrapper_get_configuration(module_get_private_data(mod), config_id,
					      MODULE_CFG_FRAGMENT_SINGLE, *data_offset_size,
					      fragment, fragment_size);
}

/**
 * \brief Sets the processing mode for the module.
 * \param[in] mod - struct processing_module pointer
 * \param[in] mode - module processing mode to be set
 *
 * \return: 0 upon success or error upon failure
 */
static int modules_set_processing_mode(struct processing_module *mod,
				       enum module_processing_mode mode)
{
	return iadk_wrapper_set_processing_mode(module_get_private_data(mod), mode);
}

/**
 * \brief Gets the processing mode actually set for the module.
 * \param[in] mod - struct processing_module pointer
 *
 * \return: enum - module processing mode value
 */
static enum module_processing_mode modules_get_processing_mode(struct processing_module *mod)
{
	return iadk_wrapper_get_processing_mode(module_get_private_data(mod));
}

/**
 * \brief Upon call to this method the ADSP system requires the module to reset its
 * internal state into a well-known initial value.
 * \param[in] mod - struct processing_module pointer
 *
 * \return: 0 upon success or error upon failure
 */
static int modules_reset(struct processing_module *mod)
{
	return iadk_wrapper_reset(module_get_private_data(mod));
}

/* Processing Module Adapter API*/
const struct module_interface processing_module_adapter_interface = {
	.init = modules_init,
	.prepare = modules_prepare,
	.process = modules_process,
	.set_processing_mode = modules_set_processing_mode,
	.get_processing_mode = modules_get_processing_mode,
	.set_configuration = modules_set_configuration,
	.get_configuration = modules_get_configuration,
	.reset = modules_reset,
	.free = modules_free,
};

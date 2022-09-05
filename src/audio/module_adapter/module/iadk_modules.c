// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@linux.intel.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/iadk_modules.h>
#include <utilities/array.h>
#include <system_agent.h>
#include <sof/lib_manager.h>

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
 * Since ProcessingModuleInterface consists of virtual functions, there are C++ -> C wrappers
 * defined to access the interface calls from SOF code.
 *
 * There are three entities in intel module adapter package:
 *  - System Agent - A mediator to allow the custom module to interact with the base SOF FW.
 *                   It calls IADK module entry point and provides all necessary information to
 *                   connect both sides of ProcessingModuleInterface and System Service.
 *  - System Service - exposes of SOF base FW services to the module.
 *  - Processing Module Adapter - SOF base FW side of ProcessingModuleInterface API
 */

/* ee2585f2-e7d8-43dc-90ab-4224e00c3e84 */
DECLARE_SOF_RT_UUID("iadk_modules", intel_uuid, 0xee2585f2, 0xe7d8, 0x43dc,
		    0x90, 0xab, 0x42, 0x24, 0xe0, 0x0c, 0x3e, 0x84);
DECLARE_TR_CTX(intel_codec_tr, SOF_UUID(intel_uuid), LOG_LEVEL_INFO);

/**
 * \brief iadk_modules_init.
 * \param[in] mod - processing module pointer.
 *
 * \return: zero on success
 *          error code on failure
 */
static int iadk_modules_init(struct processing_module *mod)
{
	uint32_t module_entry_point;
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct ipc4_base_module_cfg *src_cfg =
				(struct ipc4_base_module_cfg *)md->cfg.data;
	int ret = 0;
	byte_array_t mod_cfg;

	mod_cfg.data = md->cfg.data;
	/* Intel modules expects DW size here */
	mod_cfg.size = (md->cfg.size >> 2);
	md->private = md->cfg.data;

	struct comp_ipc_config *config = &(mod->dev->ipc_config);

	/* At this point module resources are allocated and it is moved to L2 memory. */
	module_entry_point = lib_manager_allocate_module(dev->drv, config, md->cfg.data);
	if (module_entry_point == 0) {
		comp_err(dev, "iadk_modules_init(), lib_manager_allocate_module() failed!");
		return -EINVAL;
	}
	mod->priv.module_entry_point = module_entry_point;
	comp_info(mod->dev, "iadk_modules_init() start");

	uint32_t module_id = IPC4_MOD_ID(mod->dev->ipc_config.id);
	uint32_t instance_id = IPC4_INST_ID(mod->dev->ipc_config.id);
	uint32_t log_handle = (uint32_t) mod->dev->drv->tctx;
	/* Connect loadable module interfaces with module adapter entity. */
	void *mod_adp = system_agent_start(md->module_entry_point, module_id,
					   instance_id, 0, log_handle, (void *)&mod_cfg);

	md->module_adapter = mod_adp;

	/* Allocate module buffers */
	md->mpd.in_buff = rballoc(0, SOF_MEM_CAPS_RAM, src_cfg->ibs);
	if (!md->mpd.in_buff) {
		comp_err(dev, "iadk_modules_init(): Failed to alloc in_buff");
		return -ENOMEM;
	}
	md->mpd.in_buff_size = src_cfg->ibs;

	md->mpd.out_buff = rballoc(0, SOF_MEM_CAPS_RAM, src_cfg->obs);
	if (!md->mpd.out_buff) {
		comp_err(dev, "iadk_modules_init(): Failed to alloc out_buff");
		rfree(md->mpd.in_buff);
		return -ENOMEM;
	}
	md->mpd.out_buff_size = src_cfg->obs;

	/* Call module specific init function if exists. */
	ret = iadk_wrapper_init(mod->priv.module_adapter);
	return ret;
}

/**
 * \brief iadk_modules_prepare.
 * \param[in] mod - processing module pointer.
 *
 * \return: zero on success
 *          error code on failure
 *
 * \note:   We use ipc4_base_module_cfg since this is only what we know about module
 *          configuration. Its internal structure is proprietary to the module implementation.
 *          There is one assumption - all IADK modules utilize IPC4 protocol.
 */
static int iadk_modules_prepare(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	int ret = 0;

	comp_info(dev, "iadk_modules_prepare()");

	/* Call module specific prepare function if exists. */
	ret = iadk_wrapper_prepare(mod->priv.module_adapter);
	return 0;
}

static int iadk_modules_init_process(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "iadk_modules_init_process()");

	codec->mpd.produced = 0;
	codec->mpd.consumed = 0;
	codec->mpd.init_done = 1;

	return 0;
}

/*
 * \brief iadk_modules_process.
 * \param[in] mod - processing module pointer.
 *
 * \return: zero on success
 *          error code on failure
 */
static int iadk_modules_process(struct processing_module *mod,
				struct input_stream_buffer *input_buffers,
				int num_input_buffers,
				struct output_stream_buffer *output_buffers,
				int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *md = &mod->priv;
	struct list_item *blist;
	int ret;
	int i = 0;

	if (!md->mpd.init_done)
		iadk_modules_init_process(mod);

	/* IADK modules require output buffer size to set to its real size. */
	list_for_item(blist, &dev->bsource_list) {
		mod->output_buffers[i].size = md->mpd.out_buff_size;
		i++;
	}
	/* Call module specific process function. */
	ret = iadk_wrapper_process(mod->priv.module_adapter,
				   input_buffers, num_input_buffers,
				   output_buffers, num_output_buffers);

	return ret;
}

/**
 * \brief iadk_modules_free.
 * \param[in] mod - processing module pointer.
 *
 * \return: zero on success
 *          error code on failure
 */
static int iadk_modules_free(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *md = &mod->priv;
	struct comp_ipc_config *config = &(mod->dev->ipc_config);
	int ret = 0;

	comp_info(dev, "iadk_modules_free()");
	ret = iadk_wrapper_free(mod->priv.module_adapter);
	rfree(md->mpd.in_buff);
	rfree(md->mpd.out_buff);

	/* Free module resources allocated in L2 memory. */
	ret = lib_manager_free_module(dev->drv, config);
	if (ret < 0)
		comp_err(dev, "iadk_modules_free(), lib_manager_free_module() failed!");

	return ret;
}

/*
 * \brief iadk_modules_set_configuration - Common method to assemble large configuration message
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
static int iadk_modules_set_configuration(struct processing_module *mod, uint32_t config_id,
					  enum module_cfg_fragment_position pos,
					  uint32_t data_offset_size, const uint8_t *fragment,
					  size_t fragment_size, uint8_t *response,
					  size_t response_size)
{
	return iadk_wrapper_set_configuration(mod->priv.module_adapter, config_id, pos,
					      data_offset_size, fragment, fragment_size,
					      response, response_size);
}

/*
 * \brief iadk_modules_get_configuration - Common method to retrieve module configuration
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
static int iadk_modules_get_configuration(struct processing_module *mod, uint32_t config_id,
					  enum module_cfg_fragment_position pos,
					  uint32_t data_offset_size, const uint8_t *fragment,
					  size_t fragment_size)
{
	return iadk_wrapper_get_configuration(mod->priv.module_adapter, config_id, pos,
					      data_offset_size, fragment, fragment_size);
}

/**
 * \brief Sets the processing mode for the module.
 * \param[in] mod - struct processing_module pointer
 * \param[in] mode - module processing mode to be set
 *
 * \return: 0 upon success or error upon failure
 */
static int iadk_modules_set_processing_mode(struct processing_module *mod,
					    enum module_processing_mode mode)
{
	return iadk_wrapper_set_processing_mode(mod->priv.module_adapter, mode);
}

/**
 * \brief Gets the processing mode actually set for the module.
 * \param[in] mod - struct processing_module pointer
 *
 * \return: enum - module processing mode value
 */
static enum module_processing_mode iadk_modules_get_processing_mode(struct processing_module *mod)
{
	return iadk_wrapper_get_processing_mode(mod->priv.module_adapter);
}

/**
 * \brief Upon call to this method the ADSP system requires the module to reset its
 * internal state into a well-known initial value.
 * \param[in] mod - struct processing_module pointer
 *
 * \return: 0 upon success or error upon failure
 */
static int iadk_modules_reset(struct processing_module *mod)
{
	return iadk_wrapper_reset(mod->priv.module_adapter);
}

/* Processing Module Adapter API*/
static struct module_interface iadk_interface = {
	.init  = iadk_modules_init,
	.prepare = iadk_modules_prepare,
	.process = iadk_modules_process,
	.set_processing_mode = iadk_modules_set_processing_mode,
	.get_processing_mode = iadk_modules_get_processing_mode,
	.set_configuration = iadk_modules_set_configuration,
	.get_configuration = iadk_modules_get_configuration,
	.reset = iadk_modules_reset,
	.free = iadk_modules_free,
};

/**
 * \brief Create a module adapter component.
 * \param[in] drv - component driver pointer.
 * \param[in] config - component ipc descriptor pointer.
 * \param[in] spec - pointer to module configuration data
 *
 * \return: a pointer to newly created module adapter component on success. NULL on error.
 *
 * \note: For dynamically loaded module the spec size is not known by base FW, since this is
 *        loaded module specific information. Therefore configuration size is required here.
 *        New module details are discovered during its loading, therefore comp_driver initialisation
 *        happens at this point.
 */
struct comp_dev *iadk_modules_shim_new(const struct comp_driver *drv,
				       struct comp_ipc_config *config,
				       void *spec)
{
	return module_adapter_new(drv, config, &iadk_interface, spec);
}

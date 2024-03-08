// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@linux.intel.com>
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/modules.h>
#include <utilities/array.h>
#include <system_agent.h>
#include <native_system_agent.h>
#include <api_version.h>
#include <sof/lib_manager.h>
#include <sof/audio/module_adapter/module/module_interface.h>
#include <module/module/api_ver.h>
#include <zephyr/llext/llext.h>

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
DECLARE_SOF_RT_UUID("modules", intel_uuid, 0xee2585f2, 0xe7d8, 0x43dc,
		    0x90, 0xab, 0x42, 0x24, 0xe0, 0x0c, 0x3e, 0x84);
DECLARE_TR_CTX(intel_codec_tr, SOF_UUID(intel_uuid), LOG_LEVEL_INFO);

static const struct module_interface interface;

static int modules_new(struct processing_module *mod, const void *buildinfo,
		       uintptr_t module_entry_point)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	uint32_t module_id = IPC4_MOD_ID(dev->ipc_config.id);
	uint32_t instance_id = IPC4_INST_ID(dev->ipc_config.id);
	uint32_t log_handle = (uint32_t) dev->drv->tctx;
	/* Connect loadable module interfaces with module adapter entity. */
	/* Check if native Zephyr lib is loaded */
	struct sof_man_fw_desc *desc = lib_manager_get_library_module_desc(module_id);

	if (!desc) {
		comp_err(dev, "modules_init(): Failed to load manifest");
		return -ENOMEM;
	}

	const struct sof_module_api_build_info *mod_buildinfo;

	if (buildinfo) {
		mod_buildinfo = buildinfo;
	} else {
		struct sof_man_module *module_entry =
			(struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(0));

		mod_buildinfo =
			(struct sof_module_api_build_info *)
			(module_entry->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr);
	}

	byte_array_t mod_cfg = {
		.data = (uint8_t *)md->cfg.init_data,
		/* Intel modules expects DW size here */
		.size = md->cfg.size >> 2,
	};

	/* Check if module is FDK */
	if (mod_buildinfo->format == IADK_MODULE_API_BUILD_INFO_FORMAT &&
	    mod_buildinfo->api_version_number.full == IADK_MODULE_API_CURRENT_VERSION) {
		md->module_adapter = (void *)system_agent_start(module_entry_point,
								module_id, instance_id,
								0, log_handle, &mod_cfg);
	} else if (mod_buildinfo->format == SOF_MODULE_API_BUILD_INFO_FORMAT &&
		   mod_buildinfo->api_version_number.full == SOF_MODULE_API_CURRENT_VERSION) {
		/* The module is native: start agent for sof loadable */
		mod->is_native_sof = true;
		md->ops = native_system_agent_start(mod->sys_service, module_entry_point,
						    module_id, instance_id,
						    0, log_handle, &mod_cfg);
	} else {
		return -ENOEXEC;
	}

	md->module_entry_point = module_entry_point;
	md->private = mod;

	return 0;
}

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
	const struct ipc4_base_module_cfg *src_cfg = &md->cfg.base_cfg;
	struct comp_ipc_config *config = &(dev->ipc_config);
	/* At this point module resources are allocated and it is moved to L2 memory. */
	const void *buildinfo = NULL;
	int ret;
	uintptr_t module_entry_point = lib_manager_allocate_module(mod, config, src_cfg,
								   &buildinfo);

	if (module_entry_point == 0) {
		comp_err(dev, "modules_init(), lib_manager_allocate_module() failed!");
		return -EINVAL;
	}
	comp_info(dev, "modules_init() start");

	if (!md->llext || md->ops == &interface) {
		/* First load */
		ret = modules_new(mod, buildinfo, module_entry_point);
		if (ret < 0)
			return ret;
	}

	/* Allocate module buffers */
	md->mpd.in_buff = rballoc(0, SOF_MEM_CAPS_RAM, src_cfg->ibs);
	if (!md->mpd.in_buff) {
		comp_err(dev, "modules_init(): Failed to alloc in_buff");
		return -ENOMEM;
	}
	md->mpd.in_buff_size = src_cfg->ibs;

	md->mpd.out_buff = rballoc(0, SOF_MEM_CAPS_RAM, src_cfg->obs);
	if (!md->mpd.out_buff) {
		comp_err(dev, "modules_init(): Failed to alloc out_buff");
		rfree(md->mpd.in_buff);
		return -ENOMEM;
	}
	md->mpd.out_buff_size = src_cfg->obs;

	/* Call module specific init function if exists. */
	if (mod->is_native_sof) {
		const struct module_interface *mod_in = md->ops;

		/* The order of preference */
		if (mod_in->process)
			mod->proc_type = MODULE_PROCESS_TYPE_SOURCE_SINK;
		else if (mod_in->process_audio_stream)
			mod->proc_type = MODULE_PROCESS_TYPE_STREAM;
		else if (mod_in->process_raw_data)
			mod->proc_type = MODULE_PROCESS_TYPE_RAW;
		else
			return -EINVAL;

		ret = mod_in->init(mod);
	} else {
		mod->proc_type = MODULE_PROCESS_TYPE_SOURCE_SINK;
		ret = iadk_wrapper_init(md->module_adapter);
	}

	return ret;
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
	int ret = 0;

	comp_info(dev, "modules_prepare()");

	/* Call module specific prepare function if exists. */
	if (mod->is_native_sof) {
		const struct module_interface *mod_in = mod->priv.ops;

		ret = mod_in->prepare(mod, sources, num_of_sources, sinks, num_of_sinks);
	} else {
		ret = iadk_wrapper_prepare(mod->priv.module_adapter);
	}
	return ret;
}

static int modules_init_process(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "modules_init_process()");

	codec->mpd.produced = 0;
	codec->mpd.consumed = 0;
	codec->mpd.init_done = 1;

	return 0;
}

static int modules_process(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	if (!mod->is_native_sof)
		return iadk_wrapper_process(mod->priv.module_adapter, sources,
					    num_of_sources, sinks, num_of_sinks);

	const struct module_interface *mod_in = mod->priv.ops;

	return mod_in->process(mod, sources, num_of_sources, sinks, num_of_sinks);
}

static int modules_process_audio_stream(struct processing_module *mod,
					struct input_stream_buffer *input_buffers,
					int num_input_buffers,
					struct output_stream_buffer *output_buffers,
					int num_output_buffers)
{
	if (!mod->is_native_sof)
		return -EOPNOTSUPP;

	const struct module_interface *mod_in = mod->priv.ops;

	return mod_in->process_audio_stream(mod, input_buffers, num_input_buffers,
					    output_buffers, num_output_buffers);
}

/*
 * \brief modules_process_raw.
 * \param[in] mod - processing module pointer.
 *
 * \return: zero on success
 *          error code on failure
 */
static int modules_process_raw(struct processing_module *mod,
			       struct input_stream_buffer *input_buffers,
			       int num_input_buffers,
			       struct output_stream_buffer *output_buffers,
			       int num_output_buffers)
{
	struct module_data *md = &mod->priv;

	if (!mod->is_native_sof)
		return -EOPNOTSUPP;

	if (!md->mpd.init_done)
		modules_init_process(mod);

	/* Call module specific process function. */
	const struct module_interface *mod_in = mod->priv.ops;

	return mod_in->process_raw_data(mod, input_buffers, num_input_buffers,
					output_buffers, num_output_buffers);
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
	struct module_data *md = &mod->priv;
	struct comp_ipc_config *config = &(mod->dev->ipc_config);
	int ret;

	comp_info(dev, "modules_free()");
	if (mod->is_native_sof) {
		const struct module_interface *mod_in = mod->priv.ops;

		ret = mod_in->free(mod);
	} else {
		ret = iadk_wrapper_free(mod->priv.module_adapter);
	}

	if (ret < 0)
		comp_err(dev, "Failed to free a module: %d", ret);

	rfree(md->mpd.in_buff);
	rfree(md->mpd.out_buff);

	if (!md->llext || !llext_unload(&md->llext)) {
		/* Free module resources allocated in L2 memory. */
		ret = lib_manager_free_module(mod, config);
		if (ret < 0)
			comp_err(dev, "modules_free(), lib_manager_free_module() failed!");
	}

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
	if (mod->is_native_sof) {
		const struct module_interface *mod_in = mod->priv.ops;

		return mod_in->set_configuration(mod, config_id, pos, data_offset_size, fragment,
						 fragment_size, response, response_size);
	}
	return iadk_wrapper_set_configuration(mod->priv.module_adapter, config_id, pos,
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
	if (mod->is_native_sof) {
		const struct module_interface *mod_in = mod->priv.ops;

		return mod_in->get_configuration(mod, config_id, data_offset_size,
						 fragment, fragment_size);
	}
	return iadk_wrapper_get_configuration(mod->priv.module_adapter, config_id,
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
	if (mod->is_native_sof) {
		const struct module_interface *mod_in = mod->priv.ops;

		return mod_in->set_processing_mode(mod, mode);
	}
	return iadk_wrapper_set_processing_mode(mod->priv.module_adapter, mode);
}

/**
 * \brief Gets the processing mode actually set for the module.
 * \param[in] mod - struct processing_module pointer
 *
 * \return: enum - module processing mode value
 */
static enum module_processing_mode modules_get_processing_mode(struct processing_module *mod)
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
static int modules_reset(struct processing_module *mod)
{
	if (mod->is_native_sof) {
		const struct module_interface *mod_in = mod->priv.ops;

		return mod_in->reset(mod);
	}
	return iadk_wrapper_reset(mod->priv.module_adapter);
}

/* Processing Module Adapter API*/
static const struct module_interface interface = {
	.init = modules_init,
	.prepare = modules_prepare,
	.process_raw_data = modules_process_raw,
	.process = modules_process,
	.process_audio_stream = modules_process_audio_stream,
	.set_processing_mode = modules_set_processing_mode,
	.get_processing_mode = modules_get_processing_mode,
	.set_configuration = modules_set_configuration,
	.get_configuration = modules_get_configuration,
	.reset = modules_reset,
	.free = modules_free,
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
struct comp_dev *modules_shim_new(const struct comp_driver *drv,
				  const struct comp_ipc_config *config,
				  const void *spec)
{
	return module_adapter_new(drv, config, &interface, spec);
}

// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <rtos/init.h>
#include "template.h"

/* UUID identifies the components. Use e.g. command uuidgen from package
 * uuid-runtime, add it to uuid-registry.txt in SOF top level.
 */
SOF_DEFINE_REG_UUID(template_comp);

/* Creates logging data for the component */
LOG_MODULE_REGISTER(template_comp, CONFIG_SOF_LOG_LEVEL);

/* Creates the compont trace. Traces show in trace console the component
 * info, warning, and error messages.
 */
DECLARE_TR_CTX(template_comp_tr, SOF_UUID(template_comp_uuid), LOG_LEVEL_INFO);

/**
 * template_comp_init() - Initialize the template component.
 * @mod: Pointer to module data.
 *
 * This function is called when the instance is created. The
 * macro __cold informs that the code that is non-critical
 * is loaded to slower but large DRAM.
 *
 * Return: Zero if success, otherwise error code.
 */
__cold static int template_comp_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct template_comp_comp_data *cd;

	comp_info(dev, "template_comp_init()");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	return 0;
}

/**
 * template_comp_process() - The audio data processing function.
 * @mod: Pointer to module data.
 * @sources: Pointer to audio samples data sources array.
 * @num_of_sources: Number of sources in the array.
 * @sinks: Pointer to audio samples data sinks array.
 * @num_of_sinks: Number of sinks in the array.
 *
 * This is the processing function that is called for scheduled
 * pipelines. The processing is controlled by the enable switch.
 *
 * Return: Zero if success, otherwise error code.
 */
static int template_comp_process(struct processing_module *mod,
				 struct sof_source **sources,
				 int num_of_sources,
				 struct sof_sink **sinks,
				 int num_of_sinks)
{
	struct template_comp_comp_data *cd =  module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_source *source = sources[0]; /* One input in this example */
	struct sof_sink *sink = sinks[0]; /* One output in this example */
	int frames = source_get_data_frames_available(source);
	int sink_frames = sink_get_free_frames(sink);

	comp_dbg(dev, "template_comp_process()");

	frames = MIN(frames, sink_frames);
	if (cd->enable)
		/* Process the data with the channels swap example function. */
		return cd->template_comp_func(mod, source, sink, frames);

	/* Just copy from source to sink. */
	source_to_sink_copy(source, sink, true, frames * cd->frame_bytes);
	return 0;
}

/**
 * template_comp_prepare() - Prepare the component for processing.
 * @mod: Pointer to module data.
 * @sources: Pointer to audio samples data sources array.
 * @num_of_sources: Number of sources in the array.
 * @sinks: Pointer to audio samples data sinks array.
 * @num_of_sinks: Number of sinks in the array.
 *
 * Function prepare is called just before the pipeline is started. In
 * this case the audio format parameters are for better code performance
 * saved to component data to avoid to find out them in process. The
 * processing function pointer is set to process the current audio format.
 *
 * Return: Value zero if success, otherwise error code.
 */
static int template_comp_prepare(struct processing_module *mod,
				 struct sof_source **sources, int num_of_sources,
				 struct sof_sink **sinks, int num_of_sinks)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame source_format;
	int i;

	comp_dbg(dev, "template_comp_prepare()");

	/* The processing example in this component supports one input and one
	 * output. Generally there can be more.
	 */
	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	/* get source data format */
	cd->frame_bytes = source_get_frame_bytes(sources[0]);
	cd->channels = source_get_channels(sources[0]);
	source_format = source_get_frm_fmt(sources[0]);

	/* Initialize channels order for reversing */
	for (i = 0; i < cd->channels; i++)
		cd->channel_map[i] = cd->channels - i - 1;

	cd->template_comp_func = template_comp_find_proc_func(source_format);
	if (!cd->template_comp_func) {
		comp_err(dev, "No processing function found for format %d.",
			 source_format);
		return -EINVAL;
	}

	return 0;
}

/**
 * template_comp_reset() - Reset the component.
 * @mod: Pointer to module data.
 *
 * The component reset is called when pipeline is stopped. The reset
 * should return the component to same state as init.
 *
 * Return: Value zero, always success.
 */
static int template_comp_reset(struct processing_module *mod)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "template_comp_reset()");
	memset(cd, 0, sizeof(*cd));
	return 0;
}

/**
 * template_comp_free() - Free dynamic allocations.
 * @mod: Pointer to module data.
 *
 * Component free is called when the pipelines are deleted. All
 * dynamic allocations need to be freed here. The macro __cold
 * instructs the build to locate this performance wise non-critical
 * function to large and slower DRAM.
 *
 * Return: Value zero, always success.
 */
__cold static int template_comp_free(struct processing_module *mod)
{
	assert_can_be_cold();

	comp_dbg(mod->dev, "template_comp_free()");
	return 0;
}

/* This defines the module operations */
static const struct module_interface template_comp_interface = {
	.init = template_comp_init,
	.prepare = template_comp_prepare,
	.process = template_comp_process,
	.set_configuration = template_comp_set_config,
	.get_configuration = template_comp_get_config,
	.reset = template_comp_reset,
	.free = template_comp_free
};

/* This controls build of the module. If COMP_MODULE is selected in kconfig
 * this is build as dynamically loadable module.
 */
#if CONFIG_COMP_TEMPLATE_COMP_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("TEMPLATE", &template_comp_interface, 1,
				  SOF_REG_UUID(template_comp), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(template_comp_interface, template_comp_uuid, template_comp_tr);
SOF_MODULE_INIT(template_comp, sys_comp_module_template_comp_interface_init);

#endif

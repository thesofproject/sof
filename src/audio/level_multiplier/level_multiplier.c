// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <rtos/init.h>
#include "level_multiplier.h"

/* UUID identifies the components. Use e.g. command uuidgen from package
 * uuid-runtime, add it to uuid-registry.txt in SOF top level.
 */
SOF_DEFINE_REG_UUID(level_multiplier);

/* Creates logging data for the component */
LOG_MODULE_REGISTER(level_multiplier, CONFIG_SOF_LOG_LEVEL);

/* Creates the component trace. Traces show in trace console the component
 * info, warning, and error messages.
 */
DECLARE_TR_CTX(level_multiplier_tr, SOF_UUID(level_multiplier_uuid), LOG_LEVEL_INFO);

/**
 * level_multiplier_init() - Initialize the level_multiplier component.
 * @mod: Pointer to module data.
 *
 * This function is called when the instance is created. The
 * macro __cold informs that the code that is non-critical
 * is loaded to slower but large DRAM.
 *
 * Return: Zero if success, otherwise error code.
 */
__cold static int level_multiplier_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct level_multiplier_comp_data *cd;

	comp_info(dev, "level_multiplier_init()");

	cd = mod_alloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->gain = LEVEL_MULTIPLIER_GAIN_ONE;
	return 0;
}

/**
 * level_multiplier_process() - The audio data processing function.
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
static int level_multiplier_process(struct processing_module *mod,
				    struct sof_source **sources,
				    int num_of_sources,
				    struct sof_sink **sinks,
				    int num_of_sinks)
{
	struct level_multiplier_comp_data *cd =  module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_source *source = sources[0]; /* One input */
	struct sof_sink *sink = sinks[0]; /* One output */
	int frames = source_get_data_frames_available(source);
	int sink_frames = sink_get_free_frames(sink);

	comp_dbg(dev, "level_multiplier_process()");

	frames = MIN(frames, sink_frames);
	frames = MIN(frames, dev->frames);
	if (cd->gain != LEVEL_MULTIPLIER_GAIN_ONE)
		/* Process the data with the requested gain. */
		return cd->level_multiplier_func(mod, source, sink, frames);

	/* Just copy from source to sink. */
	source_to_sink_copy(source, sink, true, frames * cd->frame_bytes);
	return 0;
}

/**
 * level_multiplier_prepare() - Prepare the component for processing.
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
static int level_multiplier_prepare(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame source_format;

	comp_dbg(dev, "level_multiplier_prepare()");

	/* The processing example in this component supports one input and one
	 * output. Generally there can be more.
	 */
	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	/* get source data format */
	cd->frame_bytes = source_get_frame_bytes(sources[0]);
	cd->channels = source_get_channels(sources[0]);
	source_format = source_get_frm_fmt(sources[0]);

	cd->level_multiplier_func = level_multiplier_find_proc_func(source_format);
	if (!cd->level_multiplier_func) {
		comp_err(dev, "No processing function found for format %d.",
			 source_format);
		return -EINVAL;
	}

	return 0;
}

/**
 * level_multiplier_reset() - Reset the component.
 * @mod: Pointer to module data.
 *
 * The component reset is called when pipeline is stopped. The reset
 * should return the component to same state as init.
 *
 * Return: Value zero, always success.
 */
static int level_multiplier_reset(struct processing_module *mod)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "level_multiplier_reset()");

	memset(cd, 0, sizeof(*cd));
	cd->gain = LEVEL_MULTIPLIER_GAIN_ONE;
	return 0;
}

/**
 * level_multiplier_free() - Free dynamic allocations.
 * @mod: Pointer to module data.
 *
 * Component free is called when the pipelines are deleted. All
 * dynamic allocations need to be freed here. The macro __cold
 * instructs the build to locate this performance wise non-critical
 * function to large and slower DRAM.
 *
 * Return: Value zero, always success.
 */
__cold static int level_multiplier_free(struct processing_module *mod)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	comp_dbg(mod->dev, "level_multiplier_free()");
	mod_free(mod, cd);
	return 0;
}

/* This defines the module operations */
static const struct module_interface level_multiplier_interface = {
	.init = level_multiplier_init,
	.prepare = level_multiplier_prepare,
	.process = level_multiplier_process,
	.set_configuration = level_multiplier_set_config,
	.reset = level_multiplier_reset,
	.free = level_multiplier_free
};

/* This controls build of the module. If COMP_MODULE is selected in kconfig
 * this is build as dynamically loadable module.
 */
#if CONFIG_COMP_LEVEL_MULTIPLIER_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(level_multiplier, &level_multiplier_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("LEVEL_MULTIPLIER", level_multiplier_llext_entry, 1,
				  SOF_REG_UUID(level_multiplier), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(level_multiplier_interface, level_multiplier_uuid, level_multiplier_tr);
SOF_MODULE_INIT(level_multiplier, sys_comp_module_level_multiplier_interface_init);

#endif

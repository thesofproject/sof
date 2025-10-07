// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <rtos/init.h>
#include "stft_process.h"

/* UUID identifies the components. Use e.g. command uuidgen from package
 * uuid-runtime, add it to uuid-registry.txt in SOF top level.
 */
SOF_DEFINE_REG_UUID(stft_process);

/* Creates logging data for the component */
LOG_MODULE_REGISTER(stft_process, CONFIG_SOF_LOG_LEVEL);

/* Creates the component trace. Traces show in trace console the component
 * info, warning, and error messages.
 */
DECLARE_TR_CTX(stft_process_tr, SOF_UUID(stft_process_uuid), LOG_LEVEL_INFO);

#if STFT_DEBUG
FILE *stft_debug_fft_in_fh;
FILE *stft_debug_fft_out_fh;
FILE *stft_debug_ifft_out_fh;
#endif

/**
 * stft_process_init() - Initialize the stft_process component.
 * @mod: Pointer to module data.
 *
 * This function is called when the instance is created. The
 * macro __cold informs that the code that is non-critical
 * is loaded to slower but large DRAM.
 *
 * Return: Zero if success, otherwise error code.
 */
__cold static int stft_process_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct stft_comp_data *cd;

	assert_can_be_cold();

	comp_info(dev, "stft_process_init()");

	cd = mod_alloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	memset(cd, 0, sizeof(*cd));

#if STFT_DEBUG
	stft_debug_fft_in_fh = fopen("stft_debug_fft_in.txt", "w");
	if (!stft_debug_fft_in_fh) {
		fprintf(stderr, "Debug file open failed.\n");
		return -EINVAL;
	}

	stft_debug_fft_out_fh = fopen("stft_debug_fft_out.txt", "w");
	if (!stft_debug_fft_out_fh) {
		fprintf(stderr, "Debug file open failed.\n");
		return -EINVAL;
	}

	stft_debug_ifft_out_fh = fopen("stft_debug_ifft_out.txt", "w");
	if (!stft_debug_ifft_out_fh) {
		fprintf(stderr, "Debug file open failed.\n");
		fclose(stft_debug_fft_out_fh);
		return -EINVAL;
	}
#endif

	return 0;
}

/**
 * stft_process_process() - The audio data processing function.
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
static int stft_process_process(struct processing_module *mod,
				struct sof_source **sources,
				int num_of_sources,
				struct sof_sink **sinks,
				int num_of_sinks)
{
	struct stft_comp_data *cd =  module_get_private_data(mod);
	struct sof_source *source = sources[0]; /* One input in this example */
	struct sof_sink *sink = sinks[0]; /* One output in this example */
	int frames = source_get_data_frames_available(source);
	int sink_frames = sink_get_free_frames(sink);
	int ret;

	frames = MIN(frames, sink_frames);

	/* Process the data with the channels swap example function. */
	ret = cd->stft_process_func(mod, source, sink, frames);

	return ret;
}

/**
 * stft_process_prepare() - Prepare the component for processing.
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
static int stft_process_prepare(struct processing_module *mod,
				struct sof_source **sources, int num_of_sources,
				struct sof_sink **sinks, int num_of_sinks)
{
	struct stft_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame source_format;
	int ret;

	comp_dbg(dev, "prepare");

	/* The processing example in this component supports one input and one
	 * output. Generally there can be more.
	 */
	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "Only one source and one sink is supported.");
		return -EINVAL;
	}

	/* Initialize STFT, max_frames is set to dev->frames + 4 */
	if (!cd->config) {
		comp_err(dev, "Can't prepare without bytes control configuration.");
		return -EINVAL;
	}

	/* get source data format */
	cd->max_frames = dev->frames + 2;
	cd->frame_bytes = source_get_frame_bytes(sources[0]);
	cd->channels = source_get_channels(sources[0]);
	source_format = source_get_frm_fmt(sources[0]);

	ret = stft_process_setup(mod, cd->max_frames, source_get_rate(sources[0]),
				 source_get_channels(sources[0]));
	if (ret < 0) {
		comp_err(dev, "setup failed.");
		return ret;
	}

	cd->stft_process_func = stft_process_find_proc_func(source_format);
	if (!cd->stft_process_func) {
		comp_err(dev, "No processing function found for format %d.",
			 source_format);
		return -EINVAL;
	}

	return 0;
}

/**
 * stft_process_reset() - Reset the component.
 * @mod: Pointer to module data.
 *
 * The component reset is called when pipeline is stopped. The reset
 * should return the component to same state as init.
 *
 * Return: Value zero, always success.
 */
static int stft_process_reset(struct processing_module *mod)
{
	struct stft_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "reset");

	stft_process_free_buffers(mod);
	memset(cd, 0, sizeof(*cd));
	return 0;
}

/**
 * stft_process_free() - Free dynamic allocations.
 * @mod: Pointer to module data.
 *
 * Component free is called when the pipelines are deleted. All
 * dynamic allocations need to be freed here. The macro __cold
 * instructs the build to locate this performance wise non-critical
 * function to large and slower DRAM.
 *
 * Return: Value zero, always success.
 */
__cold static int stft_process_free(struct processing_module *mod)
{
	struct stft_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	comp_dbg(mod->dev, "free");
	mod_free(mod, cd);

#if STFT_DEBUG
	fclose(stft_debug_fft_in_fh);
	fclose(stft_debug_fft_out_fh);
	fclose(stft_debug_ifft_out_fh);
#endif
	return 0;
}

/* This defines the module operations */
static const struct module_interface stft_process_interface = {
	.init = stft_process_init,
	.prepare = stft_process_prepare,
	.process = stft_process_process,
	.set_configuration = stft_process_set_config,
	.get_configuration = stft_process_get_config,
	.reset = stft_process_reset,
	.free = stft_process_free
};

/* This controls build of the module. If COMP_MODULE is selected in kconfig
 * this is build as dynamically loadable module.
 */
#if CONFIG_COMP_STFT_PROCESS_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("STFT_PROCESS", &stft_process_interface, 1,
				  SOF_REG_UUID(stft_process), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(stft_process_interface, stft_process_uuid, stft_process_tr);
SOF_MODULE_INIT(stft_process, sys_comp_module_stft_process_interface_init);

#endif

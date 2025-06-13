// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <rtos/init.h>
#include <user/eq.h>
#include "sound_dose.h"
#include "sound_dose_fir_48k.h"
#include "sound_dose_iir_48k.h"

/* UUID identifies the components. Use e.g. command uuidgen from package
 * uuid-runtime, add it to uuid-registry.txt in SOF top level.
 */
SOF_DEFINE_REG_UUID(sound_dose);

/* Creates logging data for the component */
LOG_MODULE_REGISTER(sound_dose, CONFIG_SOF_LOG_LEVEL);

/* Creates the compont trace. Traces show in trace console the component
 * info, warning, and error messages.
 */
DECLARE_TR_CTX(sound_dose_tr, SOF_UUID(sound_dose_uuid), LOG_LEVEL_INFO);

/**
 * sound_dose_init() - Initialize the template component.
 * @mod: Pointer to module data.
 *
 * This function is called when the instance is created. The
 * macro __cold informs that the code that is non-critical
 * is loaded to slower but large DRAM.
 *
 * Return: Zero if success, otherwise error code.
 */
__cold static int sound_dose_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct sound_dose_comp_data *cd;

	comp_info(dev, "sound_dose_init()");

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	return 0;
}

/**
 * sound_dose_process() - The audio data processing function.
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
static int sound_dose_process(struct processing_module *mod,
			      struct sof_source **sources,
			      int num_of_sources,
			      struct sof_sink **sinks,
			      int num_of_sinks)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_source *source = sources[0]; /* One input in this example */
	struct sof_sink *sink = sinks[0]; /* One output in this example */
	int frames = source_get_data_frames_available(source);
	int sink_frames = sink_get_free_frames(sink);

	comp_dbg(dev, "sound_dose_process()");

	frames = MIN(frames, sink_frames);
	return cd->sound_dose_func(mod, source, sink, frames);
}

/**
 * sound_dose_prepare() - Prepare the component for processing.
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
static int sound_dose_prepare(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_abi_hdr *blob;
	struct sof_eq_fir_config *fir_config;
	struct sof_eq_iir_config *iir_config;
	struct sof_fir_coef_data *fir_coef;
	struct sof_eq_iir_header *iir_coef;
	int32_t *data;
	enum sof_ipc_frame source_format;
	size_t fir_size, iir_size, alloc_size;
	int i;

	comp_dbg(dev, "sound_dose_prepare()");

	/* The processing example in this component supports one input and one
	 * output. Generally there can be more.
	 */
	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	/* get source data format */
	cd->frame_bytes = source_get_frame_bytes(sources[0]);
	cd->channels = source_get_channels(sources[0]);
	source_format = source_get_frm_fmt(sources[0]);

	cd->sound_dose_func = sound_dose_find_proc_func(source_format);
	if (!cd->sound_dose_func) {
		comp_err(dev, "No processing function found for format %d.",
			 source_format);
		return -EINVAL;
	}

	/* Initialize FIR */
	cd->rate = source_get_rate(sources[0]);
	switch (cd->rate) {
	case 48000:
		blob = (struct sof_abi_hdr *)sound_dose_fir_48k;
		fir_config = (struct sof_eq_fir_config *)blob->data;
		blob = (struct sof_abi_hdr *)sound_dose_iir_48k;
		iir_config = (struct sof_eq_iir_config *)blob->data;
		cd->log_offset_for_mean = SOUND_DOSE_LOG2_INV_48K_Q16;
		break;
	default:
		comp_err(dev, "error: unsupported sample rate %d");
		return -EINVAL;
	}

	/* Apply the first responses in the blobs */
	fir_coef = (struct sof_fir_coef_data *)&fir_config->data[fir_config->channels_in_config];
	iir_coef = (struct sof_eq_iir_header *)&iir_config->data[fir_config->channels_in_config];

	fir_size = fir_delay_size(fir_coef);
	iir_size = iir_delay_size_df1(iir_coef);
	alloc_size = cd->channels * (fir_size + iir_size);
	cd->delay_lines = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, alloc_size);
	if (!cd->delay_lines) {
		comp_err(dev, "Failed to allocate memory for weighting filters.");
		return -ENOMEM;
	}

	data = cd->delay_lines;
	for (i = 0; i < cd->channels; i++) {
		fir_init_coef(&cd->fir[i], fir_coef);
		fir_init_delay(&cd->fir[i], &data);
	}

	for (i = 0; i < cd->channels; i++) {
		iir_init_coef_df1(&cd->iir[i], iir_coef);
		iir_init_delay_df1(&cd->iir[i], &data);
		cd->energy[i] = 0;
	}

	cd->frames_count = 0;
	cd->report_count = cd->rate; /* report every 1s, for 48k frames */
	return 0;
}

/**
 * sound_dose_reset() - Reset the component.
 * @mod: Pointer to module data.
 *
 * The component reset is called when pipeline is stopped. The reset
 * should return the component to same state as init.
 *
 * Return: Value zero, always success.
 */
static int sound_dose_reset(struct processing_module *mod)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "sound_dose_reset()");
	memset(cd, 0, sizeof(*cd));
	return 0;
}

/**
 * sound_dose_free() - Free dynamic allocations.
 * @mod: Pointer to module data.
 *
 * Component free is called when the pipelines are deleted. All
 * dynamic allocations need to be freed here. The macro __cold
 * instructs the build to locate this performance wise non-critical
 * function to large and slower DRAM.
 *
 * Return: Value zero, always success.
 */
__cold static int sound_dose_free(struct processing_module *mod)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	comp_dbg(mod->dev, "sound_dose_free()");
	rfree(cd);
	return 0;
}

/* This defines the module operations */
static const struct module_interface sound_dose_interface = {
	.init = sound_dose_init,
	.prepare = sound_dose_prepare,
	.process = sound_dose_process,
	.set_configuration = sound_dose_set_config,
	.get_configuration = sound_dose_get_config,
	.reset = sound_dose_reset,
	.free = sound_dose_free
};

/* This controls build of the module. If COMP_MODULE is selected in kconfig
 * this is build as dynamically loadable module.
 */
#if CONFIG_COMP_TEMPLATE_COMP_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(sound_dose, &sound_dose_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
SOF_LLEXT_MODULE_MANIFEST("TEMPLATE", sound_dose_llext_entry, 1,
			  SOF_REG_UUID(sound_dose), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(sound_dose_interface, sound_dose_uuid, sound_dose_tr);
SOF_MODULE_INIT(sound_dose, sys_comp_module_sound_dose_interface_init);

#endif

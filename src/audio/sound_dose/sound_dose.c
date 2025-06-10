// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/format.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <sof/ipc/msg.h>
#include <sof/math/exp_fcn.h>
#include <sof/math/iir_df1.h>
#include <kernel/abi.h>
#include <kernel/header.h>
#include <rtos/init.h>
#include <user/eq.h>
#include <user/sound_dose.h>
#include "sound_dose.h"
#include "sound_dose_iir_44k.h"
#include "sound_dose_iir_48k.h"

/* UUID identifies the components. Use e.g. command uuidgen from package
 * uuid-runtime, add it to uuid-registry.txt in SOF top level.
 */
SOF_DEFINE_REG_UUID(sound_dose);

/* Creates logging data for the component */
LOG_MODULE_REGISTER(sound_dose, CONFIG_SOF_LOG_LEVEL);

/* Creates the component trace. Traces show in trace console the component
 * info, warning, and error messages.
 */
DECLARE_TR_CTX(sound_dose_tr, SOF_UUID(sound_dose_uuid), LOG_LEVEL_INFO);

void sound_dose_report_mel(const struct processing_module *mod)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	uint64_t tmp_h, tmp_l;

	cd->dose->current_sens_dbfs_dbspl = cd->setup.sens_dbfs_dbspl;
	cd->dose->current_volume_offset = cd->vol.volume_offset;
	cd->dose->current_gain = cd->att.gain;
	cd->dose->dbfs_value = (int32_t)(((int64_t)cd->level_dbfs * 100) >> 16);
	cd->dose->mel_value = cd->dose->dbfs_value +
		cd->setup.sens_dbfs_dbspl +
		cd->vol.volume_offset;
	/* Multiply in two 32x32 -> 64 bits parts and do sum of the parts
	 * including the shift from Q26 to Q0. The simplified 96 bits
	 * equation would be time = ((tmp_h << 32) + tmp_l) >> 26.
	 */
	tmp_l = (cd->total_frames_count & 0xffffffff) * cd->rate_to_us_coef;
	tmp_h = (cd->total_frames_count >> 32) * cd->rate_to_us_coef;
	cd->feature->stream_time_us = (tmp_l >> 26) + ((tmp_h & ((1LL << 32) - 1)) << 6);
#if SOUND_DOSE_DEBUG
	comp_info(mod->dev, "Time %d dBFS %d MEL %d",
		  (int32_t)(cd->feature->stream_time_us / 1000000),
		  cd->dose->dbfs_value, cd->dose->mel_value);
#endif

	sound_dose_send_ipc_notification(mod);
}

int sound_dose_filters_init(struct processing_module *mod)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct sof_eq_iir_config *iir_config;
	struct sof_eq_iir_header *iir_coef;
	struct comp_dev *dev = mod->dev;
	struct sof_abi_hdr *blob;
	size_t iir_size, alloc_size;
	int32_t *data;
	int i;

	/* Initialize FIR */
	switch (cd->rate) {
	case 48000:
		blob = (struct sof_abi_hdr *)sound_dose_iir_48k;
		iir_config = (struct sof_eq_iir_config *)blob->data;
		cd->log_offset_for_mean = SOUND_DOSE_LOG2_INV_48K_Q16;
		cd->rate_to_us_coef = SOUND_DOSE_1M_OVER_48K_Q26;
		break;
	case 44100:
		blob = (struct sof_abi_hdr *)sound_dose_iir_44k;
		iir_config = (struct sof_eq_iir_config *)blob->data;
		cd->log_offset_for_mean = SOUND_DOSE_LOG2_INV_44K_Q16;
		cd->rate_to_us_coef = SOUND_DOSE_1M_OVER_44K_Q26;
		break;
	default:
		/* TODO: 96 kHz and 192 kHz with integer decimation factor. The A-weight is not
		 * defined above 20 kHz, so high frequency energy is not needed. Also it will
		 * help keep the module load reasonable.
		 */
		comp_err(dev, "error: unsupported sample rate %d", cd->rate);
		return -EINVAL;
	}

	/* Apply the first responses in the blobs */
	iir_coef = (struct sof_eq_iir_header *)&iir_config->data[iir_config->channels_in_config];
	iir_size = iir_delay_size_df1(iir_coef);
	alloc_size = cd->channels * iir_size;
	cd->delay_lines = rzalloc(SOF_MEM_FLAG_USER, alloc_size);
	if (!cd->delay_lines) {
		comp_err(dev, "Failed to allocate memory for weighting filters.");
		return -ENOMEM;
	}

	data = cd->delay_lines;
	for (i = 0; i < cd->channels; i++) {
		iir_init_coef_df1(&cd->iir[i], iir_coef);
		iir_init_delay_df1(&cd->iir[i], &data);
		cd->energy[i] = 0;
	}

	cd->report_count = cd->rate; /* report every 1s, for 48k frames */
	return 0;
}

void sound_dose_filters_free(struct sound_dose_comp_data *cd)
{
	rfree(cd->delay_lines);
}

__cold static void sound_dose_setup_init(struct sound_dose_comp_data *cd)
{
	cd->setup.sens_dbfs_dbspl = 0;		/* 0 dbFS is 100 dB */
	cd->vol.volume_offset = 0;		/* Assume max vol */
	cd->att.gain = 0;			/* No attenuation, 0 dB */
	cd->gain = SOUND_DOSE_GAIN_ONE_Q30;
	cd->new_gain = SOUND_DOSE_GAIN_ONE_Q30;
}

static int sound_dose_audio_feature_init(struct processing_module *mod)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	size_t alloc_size = sizeof(struct sof_abi_hdr) +
			    sizeof(struct sof_audio_feature) +
			    sizeof(struct sof_sound_dose);

	cd->abi = rzalloc(SOF_MEM_FLAG_USER, alloc_size);
	if (!cd->abi) {
		comp_err(mod->dev, "Failed to allocate audio feature data.");
		return -ENOMEM;
	}

	cd->abi->magic = SOF_IPC4_ABI_MAGIC;
	cd->abi->abi = SOF_ABI_VERSION;
	cd->abi->size = sizeof(struct sof_audio_feature) + sizeof(struct sof_sound_dose);
	cd->feature = (struct sof_audio_feature *)cd->abi->data;
	cd->feature->data_size = sizeof(struct sof_sound_dose);
	cd->feature->type = SOF_AUDIO_FEATURE_SOUND_DOSE_MEL;
	cd->feature->num_audio_features = 1; /* Single MEL value in audio feature data */
	cd->dose = (struct sof_sound_dose *)cd->feature->data;
	return 0;
}

/**
 * sound_dose_init() - Initialize the component.
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
	int ret;

	comp_info(dev, "Initialize");

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd) {
		comp_err(dev, "Failed to allocate component data.");
		return -ENOMEM;
	}

	md->private = cd;

	sound_dose_setup_init(cd);
	ret = sound_dose_audio_feature_init(mod);
	if (ret) {
		rfree(cd);
		return ret;
	}

	ret = sound_dose_ipc_notification_init(mod);
	if (ret) {
		rfree(cd->abi);
		rfree(cd);
	}

	return ret;
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
	struct sof_source *source = sources[0]; /* One input in this example */
	struct sof_sink *sink = sinks[0]; /* One output in this example */
	struct comp_dev *dev = mod->dev;
	int32_t arg;
	int frames = source_get_data_frames_available(source);
	int sink_frames = sink_get_free_frames(sink);

	comp_dbg(dev, "sound_dose_process()");

	if (cd->gain_update) {
		/* Convert dB * 100 to Q8.24 */
		arg = (int32_t)cd->att.gain * SOUND_DOSE_ONE_OVER_100_Q24;

		/* Calculate linear value as Q12.20 and convert to Q2.30 */
		cd->new_gain = Q_SHIFT_LEFT(sofm_db2lin_fixed(arg), 20, 30);
		cd->gain_update = false;
	}

	if (cd->new_gain < cd->gain) {
		cd->gain = Q_MULTSR_32X32((int64_t)cd->gain, SOUND_DOSE_GAIN_DOWN_Q30, 30, 30, 30);
		cd->gain = MAX(cd->gain, cd->new_gain);
	} else if (cd->new_gain > cd->gain) {
		cd->gain = Q_MULTSR_32X32((int64_t)cd->gain, SOUND_DOSE_GAIN_UP_Q30, 30, 30, 30);
		cd->gain = MIN(cd->gain, SOUND_DOSE_GAIN_ONE_Q30);
	}

	frames = MIN(frames, sink_frames);
	cd->total_frames_count += frames;
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
	enum sof_ipc_frame source_format;

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
	cd->rate = source_get_rate(sources[0]);

	cd->sound_dose_func = sound_dose_find_proc_func(source_format);
	if (!cd->sound_dose_func) {
		comp_err(dev, "No processing function found for format %d.",
			 source_format);
		return -EINVAL;
	}

	return sound_dose_filters_init(mod);
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

	sound_dose_setup_init(cd);
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

	sound_dose_filters_free(cd);
	if (cd->msg) {
		rfree(cd->msg->tx_data);
		rfree(cd->msg);
	}
	rfree(cd->abi);
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
#if CONFIG_COMP_SOUND_DOSE_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("SNDDOSE", &sound_dose_interface, 1,
				  SOF_REG_UUID(sound_dose), 4);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(sound_dose_interface, sound_dose_uuid, sound_dose_tr);
SOF_MODULE_INIT(sound_dose, sys_comp_module_sound_dose_interface_init);

#endif

// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <module/base.h>
#include <module/api_ver.h>
#include <iadk/adsp_error_code.h>
#include <rimage/sof/user/manifest.h>
#include <audio/source_api.h>
#include <audio/sink_api.h>

#include "downmixer.h"

/* Logging is temporary disabled */
#define LOG_MESSAGE(...) 

static const struct native_system_agent *native_sys_agent;
static struct module_self_data self_data;

/**
 * Module specific initialization procedure.
 */
static int downmix_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	const struct module_config *dst = &mod_data->cfg;
	const struct ipc4_base_module_extended_cfg * const down_mixer = dst->init_data;
	struct up_down_mixer_data *cd;
	int i;

	if (dst->size < sizeof(*down_mixer))
	{
		LOG_MESSAGE(CRITICAL, "Invalid module config size received (%d)",
			    LOG_ENTRY, dst->size);
		return ADSP_INVALID_SETTINGS;
	}

	/* count of input pins formats */
	const size_t in_pins_format_count = down_mixer->base_cfg_ext.nb_input_pins;

	/* count of output pins formats */
	const size_t out_pins_format_count = down_mixer->base_cfg_ext.nb_output_pins;

	if (dst->size < sizeof(struct ipc4_base_module_cfg) +
	    ipc4_calc_base_module_cfg_ext_size(in_pins_format_count, out_pins_format_count))
	{
		LOG_MESSAGE(CRITICAL, "Invalid module config size received (%d)",
			    LOG_ENTRY, dst->size);
		return ADSP_INVALID_SETTINGS;
	}

	/* check that at least 1 audio format has been retrieved for 1 input pin
	 * and there are no more audio formats than the module input pins count. */
	if ((in_pins_format_count < 1) || (in_pins_format_count > INPUT_NUMBER)) {
		LOG_MESSAGE(CRITICAL, "Invalid count of input pin formats received (%d)",
			    LOG_ENTRY, in_pins_format_count);
		return ADSP_INVALID_SETTINGS;
	}

	/* check that one audio format is available for the output pin */
	if (out_pins_format_count != 1) {
		LOG_MESSAGE(CRITICAL, "Invalid count of output pin formats received (%d)",
			    LOG_ENTRY, out_pins_format_count);
		return ADSP_INVALID_SETTINGS;
	}

	const struct ipc4_input_pin_format *const input_formats =
		(const struct ipc4_input_pin_format *const)down_mixer->base_cfg_ext.pin_formats;

	const struct ipc4_output_pin_format *const output_formats =
		(const struct ipc4_output_pin_format *const)&input_formats[in_pins_format_count];

	const struct ipc4_output_pin_format output_pin_format = output_formats[0];

	/* check that output audio format is for output pin0 */
	if (output_pin_format.pin_index != 0) {
		LOG_MESSAGE(CRITICAL, "Retrieved audio format is associated to an invalid output pin index (%d)",
			    LOG_ENTRY, output_pin_format.pin_index);
		return ADSP_INVALID_SETTINGS;
	}

	/* array of input audio format indexed by the input pin index. */
	struct ipc4_input_pin_format input_pin_format[INPUT_NUMBER];

	/* initialize each ibs to 0 to indicate that pin format has not yet been configured. */
	for (int i = 0 ; i < INPUT_NUMBER ; i++)
		input_pin_format[i].ibs = 0;

	for (i = 0 ; i < in_pins_format_count ; i++) {
		const struct ipc4_input_pin_format *const pin_format = &input_formats[i];

		/* check that audio format retrieved for input is assigned
		 * to an existing module pin. */
		if (pin_format->pin_index >= INPUT_NUMBER)
		{
			LOG_MESSAGE(CRITICAL,
				    "Retrieved audio format is associated to an invalid input pin index (%d)",
				    LOG_ENTRY, pin_format->pin_index);
			return ADSP_INVALID_SETTINGS;
		}

		input_pin_format[pin_format->pin_index] = *pin_format;
	}

	/* check that at least input pin0 has an audio format */
	if (!input_pin_format[0].ibs) {
		LOG_MESSAGE(CRITICAL, "Input pin 0 is not configured", LOG_ENTRY);
		return ADSP_INVALID_SETTINGS;
	}

	/* check that input pin 0 and output pin 0 have compatible audio format */
	if (input_pin_format[0].audio_fmt.sampling_frequency !=
	    output_pin_format.audio_fmt.sampling_frequency ||
	    input_pin_format[0].audio_fmt.depth != output_pin_format.audio_fmt.depth) {
		LOG_MESSAGE(CRITICAL, "Input pin0 and output pin0 formats have incompatible audio format: "
			    "input_freq = %d, output_freq = %d, input_bit_depth = %d, output_bit_depth = %d.",
			    LOG_ENTRY, input_pin_format[0].audio_fmt.sampling_frequency,
			    output_pin_format.audio_fmt.sampling_frequency,
			    input_pin_format[0].audio_fmt.depth,
			    output_pin_format.audio_fmt.depth);
		return ADSP_INVALID_SETTINGS;
	}

	/* check that input pin 0 has a supported channels count */
	if ((input_pin_format[0].audio_fmt.channels_count!= 1)
	    && (input_pin_format[0].audio_fmt.channels_count!= 2)
	    && (input_pin_format[0].audio_fmt.channels_count!= 3)
	    && (input_pin_format[0].audio_fmt.channels_count!= 4)) {
		LOG_MESSAGE(CRITICAL, "Input pin0 format has unsupported channels count (%d)",
			    LOG_ENTRY, input_pin_format[0].audio_fmt.channels_count);
		return ADSP_INVALID_SETTINGS;
	}

	/* check that bit_depth value is supported */
	if ((output_pin_format.audio_fmt.depth != IPC4_DEPTH_16BIT) &&
	    (output_pin_format.audio_fmt.depth != IPC4_DEPTH_32BIT)) {
		LOG_MESSAGE(CRITICAL, " bit depth in audio format is not supported (%d)",
			    LOG_ENTRY, output_pin_format.audio_fmt.depth);
		return ADSP_INVALID_SETTINGS;
	}

	/* check that output pin has a supported channels count */
	if ((output_pin_format.audio_fmt.channels_count != 1)
	    && (output_pin_format.audio_fmt.channels_count != 2)) {
		LOG_MESSAGE(CRITICAL, "Output pin format has unsupported channels count (%d)",
			    LOG_ENTRY, output_pin_format.audio_fmt.channels_count);
		return ADSP_INVALID_SETTINGS;
	}

	/* check that pin 0 ibs can be divided by the bytes size of "samples group" */
	if ((input_pin_format[0].ibs * 8) % (input_pin_format[0].audio_fmt.depth *
					     input_pin_format[0].audio_fmt.channels_count)) {
		LOG_MESSAGE(CRITICAL, "ibs0*8 shall be a multiple of samples group value: "
			    "ibs = %d, input_bit_depth = %d.",
			    LOG_ENTRY, input_pin_format[0].ibs,
			    input_pin_format[0].audio_fmt.depth);
		return ADSP_INVALID_SETTINGS;
	}

	/* check that obs can be divided by the "bit_depth" settings value */
	if ((output_pin_format.obs * 8) % (output_pin_format.audio_fmt.depth *
					   output_pin_format.audio_fmt.channels_count)) {
		LOG_MESSAGE(CRITICAL, "obs0*8 shall be a multiple of samples group value"
			    "obs = %d, output_bit_depth = %d.",
			    LOG_ENTRY, output_pin_format.obs,
			    output_pin_format.audio_fmt.depth);
		return ADSP_INVALID_SETTINGS;
	}

	if (input_pin_format[1].ibs) {
		/* if some audio format is available to configure the input pin 1
		 * check that input pin 0 and pin 1 have compatible audio format */
		if ((input_pin_format[0].audio_fmt.sampling_frequency !=
		     input_pin_format[1].audio_fmt.sampling_frequency) ||
		    (input_pin_format[0].audio_fmt.depth != input_pin_format[1].audio_fmt.depth)) {
			LOG_MESSAGE(CRITICAL, "Input pin0 and input pin1 formats have incompatible audio format : "
				    "input_freq[0] = %d, input_freq[1] = %d, input_bit_depth[0] = %d, input_bit_depth[1] = %d.",
				    LOG_ENTRY, input_pin_format[0].audio_fmt.sampling_frequency,
				    input_pin_format[1].audio_fmt.sampling_frequency,
				    input_pin_format[0].audio_fmt.depth,
				    input_pin_format[1].audio_fmt.depth);
			return ADSP_INVALID_SETTINGS;
		}

		/* check that input pin 1 has a supported channels count */
		if ((input_pin_format[1].audio_fmt.channels_count != 1) &&
		    (input_pin_format[1].audio_fmt.channels_count != 2)) {
			LOG_MESSAGE(CRITICAL, "Input pin1 format has unsupported channels count (%d)",
				    LOG_ENTRY, input_pin_format[1].audio_fmt.channels_count);
			return ADSP_INVALID_SETTINGS;
		}

		/* check that pin 1 ibs can be divided by the bytes size of "samples group" */
		if ((input_pin_format[1].ibs * 8) % (input_pin_format[1].audio_fmt.depth * 
						     input_pin_format[1].audio_fmt.channels_count))
		{
			LOG_MESSAGE(CRITICAL, "ibs1*8 shall be a multiple of samples group value: "
				    "ibs = %d, input_bit_depth = %d.",
				    LOG_ENTRY, input_pin_format[1].ibs,
				    input_pin_format[1].audio_fmt.depth);
			return ADSP_INVALID_SETTINGS;
		}
	}

	const size_t input1_channels_count = (input_pin_format[1].ibs) ?
		input_pin_format[1].audio_fmt.channels_count : 0;

	/* Log BaseModuleCfgExt */
	LOG_MESSAGE(VERBOSE, "Create, in_pins_format_count = %d, out_pins_format_count = %d", LOG_ENTRY, in_pins_format_count, out_pins_format_count);
	LOG_MESSAGE(VERBOSE, "Create, input_pin_format[0]: pin_index = %d, ibs = %d", LOG_ENTRY, input_pin_format[0].pin_index, input_pin_format[0].ibs);
	LOG_MESSAGE(VERBOSE, "Create, input_pin_format[0]: freq = %d, bit_depth = %d, channel_map = %d, channel_config = %d", LOG_ENTRY, input_pin_format[0].audio_fmt.sampling_frequency, input_pin_format[0].audio_fmt.depth, input_pin_format[0].audio_fmt.ch_map, input_pin_format[0].audio_fmt.ch_cfg);
	LOG_MESSAGE(VERBOSE, "Create, input_pin_format[0]: interleaving_style = %d, number_of_channels = %d, audio_fmt.valid_bit_depth = %d, sample_type = %d", LOG_ENTRY, input_pin_format[0].audio_fmt.interleaving_style, input_pin_format[0].audio_fmt.channels_count, input_pin_format[0].audio_fmt.valid_bit_depth, input_pin_format[0].audio_fmt.s_type);
	LOG_MESSAGE(VERBOSE, "Create, input_pin_format[1]: pin_index = %d, ibs = %d", LOG_ENTRY, input_pin_format[1].pin_index, input_pin_format[1].ibs);
	LOG_MESSAGE(VERBOSE, "Create, input_pin_format[1]: freq = %d, bit_depth = %d, channel_map = %d, channel_config = %d", LOG_ENTRY, input_pin_format[1].audio_fmt.sampling_frequency, input_pin_format[1].audio_fmt.depth, input_pin_format[1].audio_fmt.ch_map, input_pin_format[1].audio_fmt.ch_cfg);
	LOG_MESSAGE(VERBOSE, "Create, input_pin_format[1]: interleaving_style = %d, number_of_channels = %d, audio_fmt.valid_bit_depth = %d, sample_type = %d", LOG_ENTRY, input_pin_format[1].audio_fmt.interleaving_style, input_pin_format[1].audio_fmt.channels_count, input_pin_format[1].audio_fmt.valid_bit_depth, input_pin_format[1].audio_fmt.s_type);
	LOG_MESSAGE(VERBOSE, "Create, output_pin_format: pin_index = %d, 0bs = %d", LOG_ENTRY, output_pin_format.pin_index, output_pin_format.obs);
	LOG_MESSAGE(VERBOSE, "Create, output_pin_format: freq = %d, bit_depth = %d, channel_map = %d, channel_config = %d", LOG_ENTRY, output_pin_format.audio_fmt.sampling_frequency, output_pin_format.audio_fmt.depth, output_pin_format.audio_fmt.ch_map, output_pin_format.audio_fmt.ch_cfg);
	LOG_MESSAGE(VERBOSE, "Create, output_pin_format: interleaving_style = %d, number_of_channels = %d, audio_fmt.valid_bit_depth = %d, sample_type = %d", LOG_ENTRY, output_pin_format.audio_fmt.interleaving_style, output_pin_format.audio_fmt.channels_count, output_pin_format.audio_fmt.valid_bit_depth, output_pin_format.audio_fmt.s_type);

	mod_data->private = &self_data;
	struct module_self_data *const self = module_get_private_data(mod);
	self->bits_per_sample_ = output_pin_format.audio_fmt.depth;
	self->input0_channels_count_ = input_pin_format[0].audio_fmt.channels_count;
	self->input1_channels_count_ = input1_channels_count;
	self->output_channels_count_ = output_pin_format.audio_fmt.channels_count;
	self->processing_mode_ = MODULE_PROCESSING_NORMAL;
	self->config_.divider_input_0 = self->input0_channels_count_ + self->input1_channels_count_;
	self->config_.divider_input_1 = self->config_.divider_input_0;

	return ADSP_NO_ERROR;
}

/**
 * Module specific processing procedure
 * This procedure is responsible to consume
 * samples provided by the module_adapter and produce/output the processed
 * ones back to module_adapter.
 *
 * there are 3 versions of the procedure, the difference is the format of
 * input/output data
 *
 * the module MUST implement one and ONLY one of them
 *
 * process_audio_stream and process_raw_data are depreciated and will be removed
 * once pipeline learns to use module API directly (without module adapter)
 * modules that need such processing should use proper wrappers
 *
 * process
 *	- sources are handlers to source API struct source*[]
 *	- sinks are handlers to sink API struct sink*[]
 */
static int downmix_process(struct processing_module *mod, struct sof_source **sources,
			   int num_of_sources, struct sof_sink **sinks, int num_of_sinks)
{
	const struct module_self_data *const self = module_get_private_data(mod);

	const uint8_t *input0_pos, *input0_start;
	const uint8_t *input1_pos, *input1_start, *input1_end;
	uint8_t *output_pos, *output_start;
	unsigned int input1_channels;
	size_t input1_frame_bytes;
	size_t frames_processed, frames_to_process;
	int32_t divider_input_0, divider_input_1;
	size_t i;
	int ret;

	volatile int x = 0;
	while (x);

	frames_to_process = sink_get_free_frames(sinks[0]);
	i = source_get_data_frames_available(sources[0]);
	if (i < frames_to_process)
		frames_to_process = i;

	const unsigned int output_channels = sink_get_channels(sinks[0]);
	const size_t output_frame_bytes = sink_get_frame_bytes(sinks[0]);
	ret = sink_get_buffer(sinks[0], frames_to_process * output_frame_bytes, (void**)&output_pos,
			      (void**)&output_start, &i);
	if (ret)
		return ADSP_FATAL_FAILURE;
	const uint8_t * const output_end = output_start + i;

	const unsigned int input0_channels = source_get_channels(sources[0]);
	const size_t input0_frame_bytes = source_get_frame_bytes(sources[0]);
	ret = source_get_data(sources[0], frames_to_process * input0_frame_bytes,
			      (const void**)&input0_pos, (const void**)&input0_start, &i);
	if (ret) {
		sink_commit_buffer(sinks[0], 0);
		return ADSP_FATAL_FAILURE;
	}
	
	const uint8_t * const input0_end = input0_start + i;

	/* If reference pin is not connected or module is in bypass mode, set input1_channels to 0.
	 * This allows to skip reference pin content in the processing loop */
	if (self->input1_channels_count_ && num_of_sources > 1 &&
	    self->processing_mode_ != MODULE_PROCESSING_NORMAL) {
		input1_channels = source_get_channels(sources[1]);
		input1_frame_bytes = source_get_frame_bytes(sources[1]);
		ret = source_get_data(sources[1], frames_to_process * input1_frame_bytes,
				      (const void**)&input1_pos,
				      (const void**)&input1_start, &i);
		if (ret) {
			sink_commit_buffer(sinks[0], 0);
			source_release_data(sources[0], 0);
			return ADSP_FATAL_FAILURE;
		}
		input1_end = input1_start + i;

	} else {
		input1_channels = 0;
		input1_frame_bytes = 0;
	}

	/* Apply processing of the input chunks and generate the output chunk */
	divider_input_0 = self->config_.divider_input_0;
	divider_input_1 = self->config_.divider_input_1;

	if (self->processing_mode_ == MODULE_PROCESSING_BYPASS) {
		divider_input_0 = self->input0_channels_count_;
		/* input1_channels is already set to 0 in BYPASS mode */
	}

	frames_processed = 0;
	while (frames_processed < frames_to_process) {
		if (self->bits_per_sample_ == IPC4_DEPTH_16BIT) {
			int16_t const *in0 = (int16_t const *)input0_pos;
			int16_t const *in1 = (int16_t const *)input1_pos;
			int16_t *out = (int16_t *)output_pos;

			int32_t mixed_sample = 0;
			for (i = 0; i < input0_channels; i++)
				mixed_sample += (int32_t)in0[i] / divider_input_0;

			for (i = 0; i < input1_channels; i++)
				mixed_sample += (int32_t)in1[i] / divider_input_1;

			for (i = 0; i < output_channels; i++)
				out[i] = (int16_t)mixed_sample;
		}

		if (self->bits_per_sample_ == IPC4_DEPTH_32BIT) {
			int32_t const *in0 = (int32_t const *)input0_pos;
			int32_t const *in1 = (int32_t const *)input1_pos;
			int32_t *out = (int32_t *)output_pos;

			int64_t mixed_sample = 0;
			for (i = 0; i < input0_channels; i++)
				mixed_sample += (int64_t)in0[i] / divider_input_0;

			for (i = 0; i < input1_channels; i++)
				mixed_sample += (int64_t)in1[i] / divider_input_1;

			for (i = 0; i < output_channels; i++)
				out[i] = (int32_t)mixed_sample;
		}

		input0_pos += input0_frame_bytes;
		if (input0_pos >= input0_end)
			input0_pos = input0_start;

		/* Pointer value is ignored if input1_channels == 0 */
		input1_pos += input1_frame_bytes;
		if (input1_pos >= input1_end)
			input1_pos = input1_start;

		output_pos += output_frame_bytes;
		if (output_pos >= output_end)
			output_pos = output_start;

		frames_processed++;
	}

	/* commit the processed data */
	source_release_data(sources[0], frames_to_process * input0_frame_bytes);
	if (input1_channels)
		source_release_data(sources[1], frames_to_process * input1_frame_bytes);
	sink_commit_buffer(sinks[0], frames_to_process * output_frame_bytes);

	return ADSP_NO_ERROR;
}

/**
 * Set module configuration for the given configuration ID
 *
 * If the complete configuration message is greater than MAX_BLOB_SIZE bytes, the
 * transmission will be split into several smaller fragments.
 * In this case the ADSP System will perform multiple calls to SetConfiguration() until
 * completion of the configuration message sending.
 * \note config_id indicates ID of the configuration message only on the first fragment
 * sending, otherwise it is set to 0.
 */
int downmix_set_configuration(struct processing_module *mod, uint32_t config_id,
			      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			      const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			      size_t response_size)
{
	struct module_self_data *const self = module_get_private_data(mod);
	const struct downmixer_config *const cfg = (const struct downmixer_config *)fragment;

	if (fragment_size < sizeof(*cfg))
		return ADSP_INVALID_CONFIGURATION;

	LOG_MESSAGE(LOW, "SetConfiguration: "
		    "config_id = %d, data_offset_size = %d, fragment_size = %d",
		    LOG_ENTRY, config_id, data_offset_size, fragment_size);

	if (cfg->divider_input_0 == 0 || cfg->divider_input_1 == 0)
		return ADSP_INVALID_CONFIGURATION;


	self->config_.divider_input_0 = cfg->divider_input_0;
	self->config_.divider_input_1 = cfg->divider_input_1;
	LOG_MESSAGE(LOW, "SetConfiguration: divider_input_0 = %d, divider_input_1 = %d",
		    LOG_ENTRY, self->config_.divider_input_0, self->config_.divider_input_1);
	return ADSP_NO_ERROR;
}

/**
 * Get module runtime configuration for the given configuration ID
 *
 * If the complete configuration message is greater than MAX_BLOB_SIZE bytes, the
 * transmission will be split into several smaller fragments.
 * In this case the ADSP System will perform multiple calls to GetConfiguration() until
 * completion of the configuration message retrieval.
 * \note config_id indicates ID of the configuration message only on the first fragment
 * retrieval, otherwise it is set to 0.
 */
int downmix_get_configuration(struct processing_module *mod, uint32_t config_id,
			      uint32_t *data_offset_size, uint8_t *fragment, size_t fragment_size)
{
	const struct module_self_data *const self = module_get_private_data(mod);
	struct downmixer_config *const cfg = (struct downmixer_config *)fragment;

	LOG_MESSAGE(LOW, "GetConfiguration: config_id(%d)", LOG_ENTRY, config_id);
	cfg->divider_input_0 = self->config_.divider_input_0;
	cfg->divider_input_1 = self->config_.divider_input_1;
	*data_offset_size = sizeof(*cfg);
	return ADSP_NO_ERROR;
}

/**
 * Set processing mode for the module
 */
static int downmix_set_processing_mode(struct processing_module *mod, enum module_processing_mode mode)
{
	struct module_self_data *const self = mod->priv.private;

	LOG_MESSAGE(LOW, "SetProcessingMode", LOG_ENTRY);

	/* Store module mode */
	self->processing_mode_ = mode;

	return 0;
}

/**
 * Get the current processing mode for the module
 */
static enum module_processing_mode downmix_get_processing_mode(struct processing_module *mod)
{
	struct module_self_data *const self = mod->priv.private;

	LOG_MESSAGE(LOW, "GetProcessingMode", LOG_ENTRY);

	return self->processing_mode_;
}

/**
 * Module specific reset procedure, called as part of module_adapter component
 * reset in .reset(). This should reset all parameters to their initial stage
 * and free all memory allocated during prepare().
 */
static int downmix_reset(struct processing_module *mod)
{
	struct module_self_data *const self = mod->priv.private;

	LOG_MESSAGE(LOW, "Reset", LOG_ENTRY);
	self->processing_mode_ = MODULE_PROCESSING_NORMAL;

	return 0;
}

static struct module_interface down_mixer_interface = {
	.init  = downmix_init,
	.process = downmix_process,
	.set_processing_mode = downmix_set_processing_mode,
 	.get_processing_mode = downmix_get_processing_mode,
	.set_configuration = downmix_set_configuration,
	.get_configuration = downmix_get_configuration,
	.reset = downmix_reset,
};

DECLARE_LOADABLE_MODULE_API_VERSION(downmix);

static void *entry_point(void *mod_cfg, void *parent_ppl, void **mod_ptr)
{
	native_sys_agent = *(const struct native_system_agent **)mod_ptr;

	return &down_mixer_interface;
}

__attribute__((section(".module")))
const struct sof_man_module_manifest downmix_manifest = {
	.module = {
		.name = "DOWNMIX",
		.uuid = {0x12, 0x34, 0xf1, 0xf1, 0x12, 0x34, 0x1a, 0x34,
			 0x8c, 0x08, 0x88, 0x4b, 0xe5, 0xd1, 0x4f, 0xaa},
		.entry_point = (uint32_t)entry_point,
		.type = { .load_type = SOF_MAN_MOD_TYPE_MODULE,
		.domain_ll = 1 },
		.affinity_mask = 1,
	}
};
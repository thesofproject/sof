// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Audio format token parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <ipc/topology.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

static const struct sof_topology_token ipc4_audio_fmt_num_tokens[] = {
	{SOF_TKN_COMP_NUM_INPUT_AUDIO_FORMATS, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t,
	 offsetof(struct sof_ipc4_available_audio_format, num_input_formats)},
	{SOF_TKN_COMP_NUM_OUTPUT_AUDIO_FORMATS, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	tplg_token_get_uint32_t,
	offsetof(struct sof_ipc4_available_audio_format, num_output_formats)},
};

static const struct sof_topology_token ipc4_in_audio_format_tokens[] = {
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_RATE, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc4_pin_format, audio_fmt.sampling_frequency)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_BIT_DEPTH, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, audio_fmt.bit_depth)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_CH_MAP, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, audio_fmt.ch_map)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_CH_CFG, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, audio_fmt.ch_cfg)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_INTERLEAVING_STYLE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t,
	 offsetof(struct sof_ipc4_pin_format, audio_fmt.interleaving_style)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_FMT_CFG, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, audio_fmt.fmt_cfg)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_INPUT_PIN_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, pin_index)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IBS, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, buffer_size)},
};

static const struct sof_topology_token ipc4_out_audio_format_tokens[] = {
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_RATE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t,
	 offsetof(struct sof_ipc4_pin_format, audio_fmt.sampling_frequency)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_BIT_DEPTH, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, audio_fmt.bit_depth)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_CH_MAP, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, audio_fmt.ch_map)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_CH_CFG, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, audio_fmt.ch_cfg)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_INTERLEAVING_STYLE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t,
	 offsetof(struct sof_ipc4_pin_format, audio_fmt.interleaving_style)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_FMT_CFG, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, audio_fmt.fmt_cfg)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUTPUT_PIN_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t, offsetof(struct sof_ipc4_pin_format, pin_index)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OBS, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
	 offsetof(struct sof_ipc4_pin_format, buffer_size)},
};

static const struct sof_topology_token ipc4_comp_pin_tokens[] = {
	{SOF_TKN_COMP_NUM_INPUT_PINS, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t, offsetof(struct tplg_pins_info, num_input_pins)},
	{SOF_TKN_COMP_NUM_OUTPUT_PINS, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t, offsetof(struct tplg_pins_info, num_output_pins)},
};

int tplg_parse_widget_audio_formats(struct tplg_context *ctx)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct sof_ipc4_available_audio_format *available_fmts = &comp_info->available_fmt;
	struct sof_ipc4_pin_format *pin_fmt;
	struct sof_ipc4_audio_format *fmt;
	int size = ctx->widget->priv.size;
	int ret, i;

	/* first parse the number of input and output pin formats */
	ret = sof_parse_token_sets(available_fmts, ipc4_audio_fmt_num_tokens,
				   ARRAY_SIZE(ipc4_audio_fmt_num_tokens), array, size, 1, 0);
	if (ret < 0) {
		fprintf(stderr, "widget: %s: Failed to parse audio_fmt_num_tokens\n",
			ctx->widget->name);
		return ret;
	}

	tplg_debug("widget: %s: number of input formats: %d number of output formats: %d\n",
		   ctx->widget->name, available_fmts->num_input_formats,
		   available_fmts->num_output_formats);

	/* allocated memory for the audio formats */
	available_fmts->output_pin_fmts =
		calloc(sizeof(struct sof_ipc4_pin_format) * available_fmts->num_output_formats, 1);
	if (!available_fmts->output_pin_fmts)
		return -ENOMEM;

	available_fmts->input_pin_fmts =
		calloc(sizeof(struct sof_ipc4_pin_format) * available_fmts->num_input_formats, 1);
	if (!available_fmts->input_pin_fmts) {
		free(available_fmts->output_pin_fmts);
		return -ENOMEM;
	}

	/* now parse the pin audio formats */
	array = &ctx->widget->priv.array[0];
	ret = sof_parse_token_sets(available_fmts->input_pin_fmts, ipc4_in_audio_format_tokens,
				   ARRAY_SIZE(ipc4_in_audio_format_tokens), array, size,
				   available_fmts->num_input_formats,
				   sizeof(struct sof_ipc4_pin_format));
	if (ret < 0) {
		fprintf(stderr, "widget: %s: failed to parse ipc4_in_audio_format_tokens\n",
			ctx->widget->name);
		return ret;
	}

	array = &ctx->widget->priv.array[0];
	ret = sof_parse_token_sets(available_fmts->output_pin_fmts, ipc4_out_audio_format_tokens,
				   ARRAY_SIZE(ipc4_out_audio_format_tokens), array, size,
				   available_fmts->num_output_formats,
				   sizeof(struct sof_ipc4_pin_format));
	if (ret < 0) {
		fprintf(stderr, "widget: %s: failed to parse ipc4_out_audio_format_tokens\n",
			ctx->widget->name);
		return ret;
	}

	/* print available audio formats */
	pin_fmt = available_fmts->output_pin_fmts;
	for (i = 0; i < available_fmts->num_output_formats; i++) {
		fmt = &pin_fmt[i].audio_fmt;
		tplg_debug("Output Pin index #%d: %uHz, %ubit (ch_map %#x ch_cfg %u interleaving_style %u fmt_cfg %#x) buffer size %d\n",
			   pin_fmt[i].pin_index, fmt->sampling_frequency, fmt->bit_depth, fmt->ch_map,
			   fmt->ch_cfg, fmt->interleaving_style, fmt->fmt_cfg,
			   pin_fmt[i].buffer_size);
	}

	pin_fmt = available_fmts->input_pin_fmts;
	for (i = 0; i < available_fmts->num_input_formats; i++) {
		fmt = &pin_fmt[i].audio_fmt;
		tplg_debug("Input Pin index #%d: %uHz, %ubit (ch_map %#x ch_cfg %u interleaving_style %u fmt_cfg %#x) buffer size %d\n",
			   pin_fmt[i].pin_index, fmt->sampling_frequency, fmt->bit_depth,
			   fmt->ch_map, fmt->ch_cfg, fmt->interleaving_style, fmt->fmt_cfg,
			   pin_fmt[i].buffer_size);
	}

	ret = sof_parse_token_sets(&comp_info->pins_info, ipc4_comp_pin_tokens,
				   ARRAY_SIZE(ipc4_comp_pin_tokens), array, size, 1, 0);
	if (ret < 0)
		fprintf(stderr, "widget: %s: Failed to parse ipc4_comp_pin_tokens\n",
			ctx->widget->name);

	return ret;
}

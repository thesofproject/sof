/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 */
#ifndef __SOF_AUDIO_TEMPLATE_COMP_H__
#define __SOF_AUDIO_TEMPLATE_COMP_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * struct template_comp_func - Function call pointer for process function
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 */
typedef int (*template_comp_func)(const struct processing_module *mod,
				  struct sof_source *source,
				  struct sof_sink *sink,
				  uint32_t frames);

/* Template_Comp component private data */

/**
 * struct template_comp_comp_data
 * @template_comp_func: Pointer to used processing function.
 * @channels_order[]: Vector with desired sink channels order.
 * @source_format: Source samples format.
 * @frame_bytes: Number of bytes in an audio frame.
 * @channels: Channels count.
 * @enable: Control processing on/off, on - reorder channels
 */
struct template_comp_comp_data {
	template_comp_func template_comp_func;
	int channel_map[PLATFORM_MAX_CHANNELS];
	int source_format;
	int frame_bytes;
	int channels;
	bool enable;
};

/**
 * struct template_comp_proc_fnmap - processing functions for frame formats
 * @frame_fmt: Current frame format
 * @template_comp_proc_func: Function pointer for the suitable processing function
 */
struct template_comp_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	template_comp_func template_comp_proc_func;
};

/**
 * template_comp_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
template_comp_func template_comp_find_proc_func(enum sof_ipc_frame src_fmt);

/**
 * template_comp_set_config() - Handle controls set
 * @mod: Pointer to module data.
 * @param_id: Id to know control type, used to know ALSA control type.
 * @pos: Position of the fragment in the large message.
 * @data_offset_size: Size of the whole configuration if it is the first or only
 *                    fragment. Otherwise it is offset of the fragment.
 * @fragment: Message payload data.
 * @fragment_size: Size of this fragment.
 * @response_size: Size of response.
 *
 * This function handles the real-time controls. The ALSA controls have the
 * param_id set to indicate the control type. The control ID, from topology,
 * is used to separate the controls instances of same type. In control payload
 * the num_elems defines to how many channels the control is applied to.
 *
 * Return: Zero if success, otherwise error code.
 */
int template_comp_set_config(struct processing_module *mod,
			     uint32_t param_id,
			     enum module_cfg_fragment_position pos,
			     uint32_t data_offset_size,
			     const uint8_t *fragment,
			     size_t fragment_size,
			     uint8_t *response,
			     size_t response_size);

/**
 * template_comp_set_config() - Handle controls get
 * @mod: Pointer to module data.
 * @config_id: Configuration ID.
 * @data_offset_size: Size of the whole configuration if it is the first or only
 *                    fragment. Otherwise it is offset of the fragment.
 * @fragment: Message payload data.
 * @fragment_size: Size of this fragment.
 *
 * This function is used for controls get.
 *
 * Return: Zero if success, otherwise error code.
 */
int template_comp_get_config(struct processing_module *mod,
			     uint32_t config_id, uint32_t *data_offset_size,
			     uint8_t *fragment, size_t fragment_size);

#endif //  __SOF_AUDIO_TEMPLATE_COMP_H__

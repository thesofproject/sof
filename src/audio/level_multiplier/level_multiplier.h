/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 */
#ifndef __SOF_AUDIO_LEVEL_MULTIPLIER_H__
#define __SOF_AUDIO_LEVEL_MULTIPLIER_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>

/** \brief Level multiplier gain Qx.y integer x number of bits including sign bit.
 * With Q8.23 format the gain range is -138.47 to +48.17 dB.
 */
#define LEVEL_MULTIPLIER_QXY_X		9

/** \brief Level multiplier gain Qx.y fractional y number of bits. */
#define LEVEL_MULTIPLIER_QXY_Y		23

/** \brief Level multiplier unity gain */
#define LEVEL_MULTIPLIER_GAIN_ONE (1 << LEVEL_MULTIPLIER_QXY_Y)

/**
 * struct level_multiplier_func - Function call pointer for process function
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 */
typedef int (*level_multiplier_func)(const struct processing_module *mod,
				     struct sof_source *source,
				     struct sof_sink *sink,
				     uint32_t frames);

/* Level_Multiplier component private data */

/**
 * struct level_multiplier_comp_data
 * @level_multiplier_func: Pointer to used processing function.
 * @gain: Applied gain in linear Q9.23 format
 * @source_format: Source samples format.
 * @frame_bytes: Number of bytes in an audio frame.
 * @channels: Channels count.
 * @enable: Control processing on/off, on - reorder channels
 */
struct level_multiplier_comp_data {
	level_multiplier_func level_multiplier_func;
	int32_t gain;
	int source_format;
	int frame_bytes;
	int channels;
};

/**
 * struct level_multiplier_proc_fnmap - processing functions for frame formats
 * @frame_fmt: Current frame format
 * @level_multiplier_proc_func: Function pointer for the suitable processing function
 */
struct level_multiplier_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	level_multiplier_func level_multiplier_proc_func;
};

/**
 * level_multiplier_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
level_multiplier_func level_multiplier_find_proc_func(enum sof_ipc_frame src_fmt);

/**
 * level_multiplier_set_config() - Handle controls set
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

#if CONFIG_IPC_MAJOR_3
static inline int level_multiplier_set_config(struct processing_module *mod,
					      uint32_t param_id,
					      enum module_cfg_fragment_position pos,
					      uint32_t data_offset_size,
					      const uint8_t *fragment,
					      size_t fragment_size,
					      uint8_t *response,
					      size_t response_size)
{
	/* No controls implementation for IPC3, add level_multiplier-ipc3.c
	 * handler if need.
	 */
	return 0;
}
#else
int level_multiplier_set_config(struct processing_module *mod,
				uint32_t param_id,
				enum module_cfg_fragment_position pos,
				uint32_t data_offset_size,
				const uint8_t *fragment,
				size_t fragment_size,
				uint8_t *response,
				size_t response_size);
#endif

#endif //  __SOF_AUDIO_LEVEL_MULTIPLIER_H__

/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 */
#ifndef __SOF_AUDIO_SOUND_DOSE_H__
#define __SOF_AUDIO_SOUND_DOSE_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/iir_df1.h>
#include <user/sound_dose.h>
#include <stdbool.h>
#include <stdint.h>

#define SOUND_DOSE_DEBUG	0

#define SOUND_DOSE_1M_OVER_44K_Q26		1521742948	/* int32(1000/44.1 * 2^26) */
#define SOUND_DOSE_1M_OVER_48K_Q26		1398101333	/* int32(1000/48 * 2^26) */
#define SOUND_DOSE_ONE_OVER_100_Q24		167772		/* int32(0.01*2^24) */
#define SOUND_DOSE_GAIN_ONE_Q30			1073741824	/* int32(2^30) */
#define SOUND_DOSE_GAIN_UP_Q30			1079940603	/* int32(10^(+0.05/20)*2^30) */
#define SOUND_DOSE_GAIN_DOWN_Q30		1067578625	/* int32(10^(-0.05/20)*2^30) */
#define SOUND_DOSE_LOG2_INV_44K_Q16		-1011122	/* int32(log2(1/44.1e3) * 2^16) */
#define SOUND_DOSE_LOG2_INV_48K_Q16		-1019134	/* int32(log2(1/48e3) * 2^16) */
#define SOUND_DOSE_TEN_OVER_LOG2_10_Q29		1616142483	/* int32(10 / log2(10) * 2^29) */
#define SOUND_DOSE_WEIGHT_FILTERS_OFFS_Q16	196608		/* int32(3 * 2^16) */
#define SOUND_DOSE_DFBS_OFFS_Q16		197263		/* int32(3.01 * 2^16) */
#define SOUND_DOSE_MEL_CHANNELS_SUM_FIX		-98304		/* int32(-1.5 * 2^16) */
#define SOUND_DOSE_ENERGY_SHIFT			19		/* Scale shift for 1s energy */
#define SOUND_DOSE_LOG_FIXED_OFFSET		(65536 * (SOUND_DOSE_ENERGY_SHIFT - 30))

#define SOUND_DOSE_S16_Q	15	/* Q1.15 samples */
#define SOUND_DOSE_S32_Q	31	/* Q1.31 samples */
#define SOUND_DOSE_GAIN_Q	30	/* Q2.30 gain */
#define SOUND_DOSE_LOGOFFS_Q	16	/* see SOUND_DOSE_LOG2_INV_48K_Q16 */
#define SOUND_DOSE_LOGMULT_Q	29	/* see SOUND_DOSE_TEN_OVER_LOG2_10_Q29 */

/**
 * struct sound_dose_func - Function call pointer for process function
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 */
typedef int (*sound_dose_func)(const struct processing_module *mod,
	struct sof_source *source,
	struct sof_sink *sink,
	uint32_t frames);

/* Sound Dose component private data */

/**
 * struct sound_dose_comp_data
 * @sound_dose_func: Pointer to used processing function.
 * @channels_order[]: Vector with desired sink channels order.
 * @source_format: Source samples format.
 * @frame_bytes: Number of bytes in an audio frame.
 * @channels: Channels count.
 * @enable: Control processing on/off, on - reorder channels
 */
struct sound_dose_comp_data {
	struct iir_state_df1 iir[PLATFORM_MAX_CHANNELS];
	struct sound_dose_setup_config setup;
	struct sound_dose_volume_config vol;
	struct sound_dose_gain_config att;
	struct sof_abi_hdr *abi;
	struct sof_audio_feature *feature;
	struct sof_sound_dose *dose;
	struct ipc_msg *msg;
	sound_dose_func sound_dose_func;
	int64_t energy[PLATFORM_MAX_CHANNELS];
	int64_t total_frames_count;
	int32_t log_offset_for_mean;
	int32_t rate_to_us_coef;
	int32_t *delay_lines;
	int32_t level_dbfs;
	int32_t new_gain;
	int32_t gain;
	bool gain_update;
	int report_count;
	int frames_count;
	int frame_bytes;
	int channels;
	int rate;
};

/**
 * struct sound_dose_proc_fnmap - processing functions for frame formats
 * @frame_fmt: Current frame format
 * @sound_dose_proc_func: Function pointer for the suitable processing function
 */
struct sound_dose_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	sound_dose_func sound_dose_proc_func;
};

/**
 * sound_dose_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
sound_dose_func sound_dose_find_proc_func(enum sof_ipc_frame src_fmt);

/**
 * sound_dose_set_config() - Handle controls set request
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
int sound_dose_set_config(struct processing_module *mod,
			  uint32_t param_id,
			  enum module_cfg_fragment_position pos,
			  uint32_t data_offset_size,
			  const uint8_t *fragment,
			  size_t fragment_size,
			  uint8_t *response,
			  size_t response_size);

/**
 * sound_dose_get_config() - Handle controls get request
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
int sound_dose_get_config(struct processing_module *mod,
			  uint32_t config_id, uint32_t *data_offset_size,
			  uint8_t *fragment, size_t fragment_size);

void sound_dose_filters_free(struct sound_dose_comp_data *cd);
int sound_dose_filters_init(struct processing_module *mod);
void sound_dose_report_mel(const struct processing_module *mod);
int sound_dose_ipc_notification_init(struct processing_module *mod);
void sound_dose_send_ipc_notification(const struct processing_module *mod);

#endif /*  __SOF_AUDIO_SOUND_DOSE_H__ */

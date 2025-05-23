/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 */

#ifndef ADSP_FW_MIC_PRIVACY_MANAGER_H
#define ADSP_FW_MIC_PRIVACY_MANAGER_H

#include <zephyr/drivers/mic_privacy/intel/mic_privacy.h>
#include <sof/lib/notifier.h>
#include <ipc4/base-config.h>
#include "../audio/copier/copier_gain.h"

#define ADSP_RTC_FREQUENCY	32768

struct mic_privacy_data {
	enum ipc4_sampling_frequency audio_freq;
	uint32_t mic_privacy_state;
	bool dma_data_zeroing;
	uint32_t fade_in_out_bytes;
	uint32_t max_ramp_time_in_ms;

	struct copier_gain_params mic_priv_gain_params;
};

struct mic_privacy_settings {
	enum mic_privacy_policy mic_privacy_mode;
	/* 0-Mic Unmute, 1-Mic Mute */
	uint32_t mic_privacy_state;
	uint32_t max_ramp_time;
	union mic_privacy_mask privacy_mask_bits;
};

struct privacy_capabilities {
	uint32_t privacy_version;
	uint32_t capabilities_length;
	uint32_t capabilities[1];
};

enum mic_privacy_state {
	MIC_PRIV_UNMUTED,
	MIC_PRIV_FADE_IN,
	MIC_PRIV_FADE_OUT,
	MIC_PRIV_MUTED,
};

int mic_privacy_manager_init(void);
int mic_privacy_manager_get_policy(void);
uint32_t mic_privacy_get_policy_register(void);
void mic_privacy_propagate_settings(struct mic_privacy_settings *settings);
uint32_t mic_privacy_get_dma_zeroing_wait_time(void);
uint32_t mic_privacy_get_privacy_mask(void);
uint32_t mic_privacy_get_mic_disable_status(void);
void mic_privacy_enable_dmic_irq(bool enable_irq);
void mic_privacy_fill_settings(struct mic_privacy_settings *settings, uint32_t mic_disable_status);
void mic_privacy_set_gtw_mic_state(struct mic_privacy_data *mic_priv_data,
				   uint32_t mic_disable_status);
void mic_privacy_update_gtw_mic_state(struct mic_privacy_data *mic_priv_data,
				      uint32_t hw_mic_disable_status);
void mic_privacy_process(struct comp_dev *dev, struct mic_privacy_data *mic_priv,
			 struct comp_buffer *buffer, uint32_t copy_samples);
void mic_privacy_gain_input(uint8_t *buff, uint32_t buff_size,  uint32_t mic_priv_state,
			    const struct ipc4_audio_format *in_fmt);

#endif /* ADSP_FW_MIC_PRIVACY_MANAGER_H */

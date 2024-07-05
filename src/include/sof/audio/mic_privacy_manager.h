// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.

#ifndef ADSP_FW_MIC_PRIVACY_MANAGER_H
#define ADSP_FW_MIC_PRIVACY_MANAGER_H

#include <zephyr/drivers/mic_privacy.h>
#include <sof/lib/notifier.h>
#include <xtensa/tie/xt_hifi4.h>
#include <ipc4/base-config.h>

#define ADSP_RTC_FREQUENCY                         32768


struct mic_privacy_data {
	enum ipc4_sampling_frequency audio_freq;
	uint32_t mic_privacy_state;
	bool dma_data_zeroing;
	uint32_t fade_in_out_bytes;
	uint32_t max_ramp_time_in_ms;

	//struct gain_params mic_priv_gain_params;
	//struct gain_ioctl mic_priv_gain_coefs_ioctl;
};


struct mic_privacy_settings {
	enum mic_privacy_policy mic_privacy_mode;
	/* 0-Mic Unmute, 1-Mic Mute */
	uint32_t mic_privacy_state;
	uint32_t max_ramp_time;
	union privacy_mask privacy_mask_bits;

};

struct privacy_capabilities {
	uint32_t privacy_version;
	uint32_t capabilities_length;
	uint32_t capabilities[1];
};



//describes gain direction
enum mic_priv_gain_direction
{
	ADDITION            = 0,
	SUBTRACTION         = 1,
};

//describes gain states

enum mic_priv_gain_state
{
	TRANS_MUTE_MIC = 0,
	TRANS_GAIN_MIC = 1,
	STATIC_GAIN_MIC = 2,
	NONE = 3,
};


enum mic_privacy_state {
	UNMUTED           = 0,
	FADE_IN           = 1,
	FADE_OUT          = 2,
	MUTED             = 3,
};


int mic_privacy_manager_init(void);

int mic_privacy_manager_get_policy(void);

uint32_t mic_privacy_get_policy_register(void);

void enable_fw_managed_irq(bool enable_irq);

void handle_dmic_interrupt(void * const self, int a, int b);

void handle_fw_managed_interrupt(void * const dev);

void propagate_privacy_settings(struct mic_privacy_settings *mic_privacy_settings);

uint32_t get_dma_zeroing_wait_time(void);

uint32_t get_privacy_mask(void);

struct mic_privacy_settings fill_mic_priv_settings(uint32_t mic_disable_status);

void set_gtw_mic_state(struct mic_privacy_data *mic_priv_data, uint32_t mic_disable_status);

void update_gtw_mic_state(struct mic_privacy_data *mic_priv_data, uint32_t hw_mic_disable_status);

void mic_privacy_process(struct mic_privacy_data *mic_priv, struct comp_buffer *buffer, uint32_t copy_samples);

void mic_priv_gain_input(uint8_t *buff, uint32_t buff_size,  uint32_t mic_priv_state,
					const struct ipc4_audio_format *in_fmt);

#endif /* ADSP_FW_MIC_PRIVACY_MANAGER_H */

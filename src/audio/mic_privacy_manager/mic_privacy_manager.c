// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.


#include "sof/audio/mic_privacy_manager.h"


#include <zephyr/device.h>
#include <zephyr/drivers/mic_privacy.h>
#include <zephyr/logging/log.h>

const struct device *mic_priv_dev;

struct mic_privacy_api_funcs *mic_privacy_api;
enum mic_privacy_policy mic_privacy_policy;

#define LOG_DOMAIN mic_priv

LOG_MODULE_REGISTER(LOG_DOMAIN);

#define MAX_INT64  (0x7FFFFFFFFFFFFFFFULL)


int mic_privacy_manager_init(void)
{
	LOG_INF("mic_privacy_manager_init");

	mic_priv_dev = DEVICE_DT_GET(DT_NODELABEL(mic_privacy));

	if (mic_priv_dev == NULL)
		return -EINVAL;

	mic_privacy_api = (struct mic_privacy_api_funcs *) mic_priv_dev->api;

	mic_privacy_policy = mic_privacy_api->get_policy();

	if (mic_privacy_policy == FW_MANAGED)
	{
		LOG_INF("mic_privacy FW_MANAGED");

		mic_privacy_api->set_fw_managed_mode(true);
	}

	//Enable interrupts
	if (mic_privacy_policy == HW_MANAGED) {
	               LOG_INF("mic_privacy init HW_MANAGED");
	       }

	else {
		enable_fw_managed_irq(true);
	}

	return 0;
}

int mic_privacy_manager_get_policy(void)
{
	LOG_INF("mic_privacy_manager_get_policy ");

	mic_privacy_api = (struct mic_privacy_api_funcs *) mic_priv_dev->api;

	return mic_privacy_api->get_policy();
}

uint32_t mic_privacy_get_policy_register(void)
{
	if (mic_priv_dev == NULL)
		return -EINVAL;
	mic_privacy_api = (struct mic_privacy_api_funcs *) mic_priv_dev->api;

	return mic_privacy_api->get_privacy_policy_register_raw_value();
}

void enable_fw_managed_irq(bool enable_irq)
{
	LOG_INF("enable_fw_managed_irq %d", enable_irq);

	if (enable_irq)
		mic_privacy_api->enable_fw_managed_irq(true, handle_fw_managed_interrupt);
	else
		mic_privacy_api->enable_fw_managed_irq(false, NULL);
}

void handle_dmic_interrupt(void *const self, int a, int b)
{
	LOG_INF("mic_privacy handle_dmic_interrupt");
	//TODO
}

void mic_priv_get_disable_stat(int num)
{

	uint32_t mic_disable_status = mic_privacy_api->get_fw_managed_mic_disable_status();

	if(mbu_mic_stat != mic_disable_status)
		LOG_INF("mic_priv_get_disable_stat(%d) = 0x%x STATE CHANGE", num, mic_disable_status);

}

void handle_fw_managed_interrupt(void * const dev)
{
	LOG_INF("handle_fw_managed_interrupt");
	uint32_t mic_disable_status = mic_privacy_api->get_fw_managed_mic_disable_status();


	struct mic_privacy_settings mic_privacy_settings =
					fill_mic_priv_settings(mic_disable_status);

	propagate_privacy_settings(&mic_privacy_settings);


	if (mic_disable_status)
		mic_privacy_api->set_fw_mic_disable_status(true);
	else
		mic_privacy_api->set_fw_mic_disable_status(false);

	mic_privacy_api->clear_fw_managed_irq();
}

void propagate_privacy_settings(struct mic_privacy_settings *mic_privacy_settings)
{
	LOG_INF("propagate_privacy_settings");
	notifier_event(mic_priv_dev, NOTIFIER_ID_MIC_PRIVACY_STATE_CHANGE,
		NOTIFIER_TARGET_CORE_ALL_MASK,
		mic_privacy_settings, sizeof(struct mic_privacy_settings));

}

uint32_t get_dma_zeroing_wait_time(void)
{
	return (uint32_t)mic_privacy_api->get_dma_data_zeroing_wait_time();
}


uint32_t get_privacy_mask(void)
{
	LOG_INF("get_privacy_mask ");
	//hardcoded for PTL
	return 0xFFFFFFFF;

}

struct mic_privacy_settings fill_mic_priv_settings(uint32_t mic_disable_status)
{
	LOG_INF("fill_mic_priv_settings");

	struct mic_privacy_settings mic_privacy_settings = {};

	mic_privacy_settings.mic_privacy_mode = mic_privacy_policy;
	mic_privacy_settings.mic_privacy_state = mic_disable_status;
	mic_privacy_settings.privacy_mask_bits.value = get_privacy_mask();
	mic_privacy_settings.max_ramp_time = get_dma_zeroing_wait_time();

	LOG_INF("mic_privacy_mode = %d, mic_disable_status = %d, privacy_mask = 0x%x, max_ramp_time_in_ms = %d",
	        mic_privacy_settings.mic_privacy_mode,
	        mic_privacy_settings.mic_privacy_state,
	        mic_privacy_settings.privacy_mask_bits.value,
	        mic_privacy_settings.max_ramp_time );

	return mic_privacy_settings;
}

void set_gtw_mic_state(struct mic_privacy_data *mic_priv_data,  uint32_t mic_disable_status)
{

	if (mic_privacy_policy == HW_MANAGED) {
		if (mic_disable_status != 0) {

		} else {
			mic_priv_data->mic_privacy_state = UNMUTED;
		}
	} else if (mic_privacy_policy == FW_MANAGED) {
		LOG_INF("set_gtw_mic_state FW_MANAGED, mic_disable_status = %d",
			mic_disable_status);

		if (mic_disable_status != 0) {
			LOG_INF("set_gtw_mic_state MUTED");
			mic_priv_data->mic_privacy_state = MUTED;
			mic_priv_data->dma_data_zeroing = true;
			mic_privacy_api->set_fw_mic_disable_status(true);
		} else {
			LOG_INF("set_gtw_mic_state UNMUTED");
			mic_priv_data->mic_privacy_state = UNMUTED;
			mic_priv_data->dma_data_zeroing = false;
			mic_privacy_api->set_fw_mic_disable_status(false);
		}
	}
}


void update_gtw_mic_state(struct mic_privacy_data *mic_priv_data, uint32_t hw_mic_disable_status)
{

	switch (mic_privacy_policy) {
	case HW_MANAGED:
		{
			LOG_INF("update_gtw_mic_state HW_MANAGED");
			//set_gtw_mic_state(source_gtw, hw_mic_disable_status);
			break;
		}
	case FW_MANAGED:
		{
			LOG_INF("update_gtw_mic_state FW_MANAGED");
			set_gtw_mic_state(mic_priv_data,
				mic_privacy_api->get_fw_managed_mic_disable_status());
			break;
		}
	default:
		{
			break;
		}
	}
}


void mic_privacy_process(struct mic_privacy_data *mic_priv, struct comp_buffer *buffer, uint32_t copy_samples)
{

	uint32_t sg_size_in_bytes;
	sg_size_in_bytes = audio_stream_frame_bytes(&buffer->stream);
	uint32_t one_ms_in_bytes = sg_size_in_bytes * (buffer->stream.runtime_stream_params.rate / 1000);
	uint32_t copy_bytes = copy_samples * audio_stream_sample_bytes(&buffer->stream);

	if (mic_priv->mic_privacy_state == FADE_IN) {
		if (mic_priv->fade_in_out_bytes == 0) {
			//start addition
			//mic_priv->mic_priv_gain_params.fade_in_sg_count = 0;
			//mic_priv->mic_priv_gain_params.gain_env = 0;

			if (mic_privacy_manager_get_policy() == FW_MANAGED) {

				//TODO set hda hw bit
				//REGISTER(DGLISxCS)(stream_idx)->part.ddz = 1;
				//REGISTER(DGLISxCS)(stream_idx)->part.ddz = 0;
			}
		}
		mic_priv->fade_in_out_bytes += copy_bytes;
		if (mic_priv->fade_in_out_bytes > one_ms_in_bytes * mic_priv->max_ramp_time_in_ms) {
			mic_priv->mic_privacy_state = UNMUTED;
			mic_priv->fade_in_out_bytes = 0;
		}

		if (mic_priv->max_ramp_time_in_ms > 0) {
			//gain_input(buffer, &mic_priv->mic_priv_gain_params, &mic_priv->mic_priv_gain_coefs_ioctl, copy_bytes);
			data_zeroing(buffer); //Gain temporarily disabled
		}
	}
	else if (mic_priv->mic_privacy_state == FADE_OUT){
		if (mic_priv->fade_in_out_bytes == 0) {
			//start subtraction

			//mic_priv->mic_priv_gain_params.fade_in_sg_count = 0;
			//mic_priv->mic_priv_gain_params.gain_env = MAX_INT64;
		}
		mic_priv->fade_in_out_bytes += copy_bytes;
		if (mic_priv->fade_in_out_bytes > one_ms_in_bytes * mic_priv->max_ramp_time_in_ms) {
			mic_priv->mic_privacy_state = MUTED;
			mic_priv->fade_in_out_bytes = 0;
		}
		if (mic_priv->max_ramp_time_in_ms > 0) {
			//LOG_INF("FADE_OUT dmic_gain_input ramp time ms = %d", mic_priv->max_ramp_time_in_ms);
			//gain_input(buffer, &mic_priv->mic_priv_gain_params, &mic_priv->mic_priv_gain_coefs_ioctl, copy_bytes);
			data_zeroing(buffer); //Gain temporarily disabled
		}
	}
	else if (mic_priv->mic_privacy_state == MUTED) {
		data_zeroing(buffer);
	}
}


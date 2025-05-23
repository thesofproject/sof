// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#include "sof/audio/mic_privacy_manager.h"

#include <zephyr/device.h>
#include <zephyr/drivers/mic_privacy/intel/mic_privacy.h>
#include <zephyr/logging/log.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/mic_privacy_manager.h>

const struct device *mic_priv_dev;

static struct mic_privacy_api_funcs *mic_privacy_api;
static enum mic_privacy_policy mic_privacy_policy;

#define LOG_DOMAIN mic_priv

LOG_MODULE_REGISTER(LOG_DOMAIN);

void handle_dmic_irq(void const *self, int a, int b)
{
	LOG_DBG("mic_privacy DMIC IRQ");

	if (mic_privacy_api->get_dmic_irq_status()) {
		uint32_t mic_disable_status = mic_privacy_api->get_dmic_mic_disable_status();
		struct mic_privacy_settings settings;

		mic_privacy_fill_settings(&settings, mic_disable_status);
		mic_privacy_propagate_settings(&settings);
		mic_privacy_api->clear_dmic_irq_status();
	}
}

void handle_fw_managed_irq(void const *dev)
{
	LOG_DBG("mic_privacy FW Managed IRQ");

	uint32_t mic_disable_status = mic_privacy_api->get_fw_managed_mic_disable_status();
	struct mic_privacy_settings settings;

	mic_privacy_fill_settings(&settings, mic_disable_status);
	mic_privacy_propagate_settings(&settings);

	if (mic_disable_status)
		mic_privacy_api->set_fw_mic_disable_status(true);
	else
		mic_privacy_api->set_fw_mic_disable_status(false);

	mic_privacy_api->clear_fw_managed_irq();
}

static void enable_fw_managed_irq(bool enable_irq)
{
	if (enable_irq)
		mic_privacy_api->enable_fw_managed_irq(true, handle_fw_managed_irq);
	else
		mic_privacy_api->enable_fw_managed_irq(false, NULL);
}

void mic_privacy_enable_dmic_irq(bool enable_irq)
{
	/* Only proceed if we have a valid device and API */
	if (!mic_priv_dev || !mic_privacy_api) {
		LOG_ERR("mic_privacy device or API not initialized");
		return;
	}

	if (mic_privacy_api->get_policy() == MIC_PRIVACY_HW_MANAGED) {
		if (enable_irq) {
			mic_privacy_api->enable_dmic_irq(true, handle_dmic_irq);

			/* Check current status immediately to handle any transitions during D3 */
			if (mic_privacy_api->get_dmic_irq_status()) {
				struct mic_privacy_settings settings;
				uint32_t mic_disable_status =
					mic_privacy_api->get_dmic_mic_disable_status();

				mic_privacy_fill_settings(&settings, mic_disable_status);
				mic_privacy_propagate_settings(&settings);
				mic_privacy_api->clear_dmic_irq_status();
			}
		} else {
			mic_privacy_api->enable_dmic_irq(false, NULL);
		}
	}
}

int mic_privacy_manager_init(void)
{
	mic_priv_dev = DEVICE_DT_GET(DT_NODELABEL(mic_privacy));

	if (!mic_priv_dev)
		return -EINVAL;

	mic_privacy_api = (struct mic_privacy_api_funcs *)mic_priv_dev->api;
	mic_privacy_policy = mic_privacy_api->get_policy();

	if (mic_privacy_policy == MIC_PRIVACY_FW_MANAGED) {
		LOG_INF("mic_privacy init FW_MANAGED mode");
		mic_privacy_api->set_fw_managed_mode(true);
		enable_fw_managed_irq(true);
	}

	return 0;
}

int mic_privacy_manager_get_policy(void)
{
	mic_privacy_api = (struct mic_privacy_api_funcs *)mic_priv_dev->api;

	return mic_privacy_api->get_policy();
}

uint32_t mic_privacy_get_policy_register(void)
{
	if (!mic_priv_dev)
		return 0;
	mic_privacy_api = (struct mic_privacy_api_funcs *)mic_priv_dev->api;

	return mic_privacy_api->get_privacy_policy_register_raw_value();
}

void mic_privacy_propagate_settings(struct mic_privacy_settings *settings)
{
	notifier_event(mic_priv_dev, NOTIFIER_ID_MIC_PRIVACY_STATE_CHANGE,
		       NOTIFIER_TARGET_CORE_ALL_MASK, settings,
		       sizeof(struct mic_privacy_settings));
}

uint32_t mic_privacy_get_dma_zeroing_wait_time(void)
{
	return (uint32_t)mic_privacy_api->get_dma_data_zeroing_wait_time();
}

uint32_t mic_privacy_get_privacy_mask(void)
{
	if (mic_privacy_policy == MIC_PRIVACY_HW_MANAGED)
		return mic_privacy_api->get_dma_data_zeroing_link_select();

	/* hardcoded for FW_MANAGED */
	return 0xFFFFFFFF;
}

void mic_privacy_fill_settings(struct mic_privacy_settings *settings, uint32_t mic_disable_status)
{
	if (!settings) {
		LOG_ERR("Invalid mic_privacy_settings pointer");
		return;
	}

	settings->mic_privacy_mode = mic_privacy_policy;
	settings->mic_privacy_state = mic_disable_status;
	settings->privacy_mask_bits.value = mic_privacy_get_privacy_mask();
	settings->max_ramp_time = mic_privacy_get_dma_zeroing_wait_time();

	LOG_DBG("mic_privacy_mode = %d, mic_disable_status = %d, \
		privacy_mask = %x, max_ramp_time_in_ms = %d",
		settings->mic_privacy_mode,
		settings->mic_privacy_state,
		settings->privacy_mask_bits.value,
		settings->max_ramp_time);
}

void mic_privacy_set_gtw_mic_state(struct mic_privacy_data *mic_priv_data,
				   uint32_t mic_disable_status)
{
	if (mic_privacy_policy == MIC_PRIVACY_HW_MANAGED) {
		if (mic_disable_status != 0)
			mic_priv_data->mic_privacy_state = MIC_PRIV_MUTED;
		else
			mic_priv_data->mic_privacy_state = MIC_PRIV_UNMUTED;
	} else if (mic_privacy_policy == MIC_PRIVACY_FW_MANAGED) {
		if (mic_disable_status != 0) {
			LOG_DBG("MUTED");
			mic_priv_data->mic_privacy_state = MIC_PRIV_MUTED;
			mic_priv_data->dma_data_zeroing = true;
			mic_privacy_api->set_fw_mic_disable_status(true);
		} else {
			LOG_DBG("UNMUTED");
			mic_priv_data->mic_privacy_state = MIC_PRIV_UNMUTED;
			mic_priv_data->dma_data_zeroing = false;
			mic_privacy_api->set_fw_mic_disable_status(false);
		}
	}
}

void mic_privacy_update_gtw_mic_state(struct mic_privacy_data *mic_priv_data,
				      uint32_t hw_mic_disable_status)
{
	switch (mic_privacy_policy) {
	case MIC_PRIVACY_HW_MANAGED:
		mic_privacy_set_gtw_mic_state(mic_priv_data, hw_mic_disable_status);
		break;
	case MIC_PRIVACY_FW_MANAGED:
		mic_privacy_set_gtw_mic_state(mic_priv_data,
					      mic_privacy_api->get_fw_managed_mic_disable_status());
		break;
	default:
		break;
	}
}

void mic_privacy_process(struct comp_dev *dev, struct mic_privacy_data *mic_priv,
			 struct comp_buffer *buffer, uint32_t copy_samples)
{
	uint32_t sg_size_in_bytes;

	sg_size_in_bytes = audio_stream_frame_bytes(&buffer->stream);
	uint32_t one_ms_in_bytes = sg_size_in_bytes * (buffer->stream.runtime_stream_params.rate / 1000);
	uint32_t copy_bytes = copy_samples * audio_stream_sample_bytes(&buffer->stream);

	switch (mic_priv->mic_privacy_state) {
	case MIC_PRIV_UNMUTED:
		break;
	case MIC_PRIV_MUTED:
		buffer_zero(buffer);
		break;
	case MIC_PRIV_FADE_IN:
		if (mic_priv->fade_in_out_bytes == 0) {
			/* start addition */
			mic_priv->mic_priv_gain_params.fade_in_sg_count = 0;
			mic_priv->mic_priv_gain_params.gain_env = 0;
		}
		mic_priv->fade_in_out_bytes += copy_bytes;
		if (mic_priv->fade_in_out_bytes > one_ms_in_bytes * mic_priv->max_ramp_time_in_ms) {
			mic_priv->mic_privacy_state = MIC_PRIV_UNMUTED;
			mic_priv->fade_in_out_bytes = 0;
		}

		if (mic_priv->max_ramp_time_in_ms > 0)
			copier_gain_input(dev, buffer, &mic_priv->mic_priv_gain_params, GAIN_ADD, copy_bytes);
		break;
	case MIC_PRIV_FADE_OUT:
		if (mic_priv->fade_in_out_bytes == 0) {
			/* start subtraction */
			mic_priv->mic_priv_gain_params.fade_in_sg_count = 0;
			mic_priv->mic_priv_gain_params.gain_env = INT64_MAX;
		}
		mic_priv->fade_in_out_bytes += copy_bytes;
		if (mic_priv->fade_in_out_bytes > one_ms_in_bytes * mic_priv->max_ramp_time_in_ms) {
			mic_priv->mic_privacy_state = MIC_PRIV_MUTED;
			mic_priv->fade_in_out_bytes = 0;
			buffer_zero(buffer);
		}
		if (mic_priv->max_ramp_time_in_ms > 0)
			copier_gain_input(dev, buffer, &mic_priv->mic_priv_gain_params, GAIN_SUBTRACT, copy_bytes);
		break;
	default:
		LOG_ERR("invalid state %x", mic_priv->mic_privacy_state);
		break;
	}
}

uint32_t mic_privacy_get_mic_disable_status(void)
{
	if (!mic_priv_dev) {
		LOG_ERR("mic_privacy device not initialized");
		return 0;
	}

	mic_privacy_api = (struct mic_privacy_api_funcs *)mic_priv_dev->api;
	if (mic_privacy_api->get_policy() == MIC_PRIVACY_FW_MANAGED)
		return mic_privacy_api->get_fw_managed_mic_disable_status();

	return mic_privacy_api->get_dmic_mic_disable_status();
}

/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __SOF_AUDIO_PIPELINE_SOF_STATIC_PIPELINE_H__
#define __SOF_AUDIO_PIPELINE_SOF_STATIC_PIPELINE_H__

#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sof_audio_interface {
	SOF_AUDIO_IF_I2S = 0,
	SOF_AUDIO_IF_PDM = 1,
};

enum sof_clock_mode {
	SOF_CLOCK_SLAVE = 0,
	SOF_CLOCK_MASTER = 1,
};

struct sof_static_pipeline_status {
	bool playback_active;
	bool capture_active;
	uint32_t sample_rate;
	enum sof_audio_interface active_interface;
	enum sof_clock_mode clock_mode;
	int16_t playback_volume;
	bool playback_mute;
	int16_t capture_volume;
	bool capture_mute;
	bool eq_playback_bypassed;
	bool drc_playback_bypassed;
	bool tdfb_capture_bypassed;
	bool eq_capture_bypassed;
};

int sof_static_pipelines_init(struct sof *sof);
int sof_static_pipeline_set_clock_mode(enum sof_audio_interface iface, enum sof_clock_mode mode);
int sof_static_pipeline_set_eq_bypass(bool is_capture, bool bypass);
int sof_static_pipeline_set_drc_bypass(bool bypass);
int sof_static_pipeline_set_tdfb_bypass(bool bypass);
int sof_static_pipeline_set_volume(uint32_t pipeline_id, int16_t volume);
int sof_static_pipeline_set_mute(uint32_t pipeline_id, bool mute);
int sof_static_pipeline_get_volume(uint32_t pipeline_id, int16_t *volume);
int sof_static_pipeline_get_mute(uint32_t pipeline_id, bool *mute);
void sof_static_pipeline_get_status(struct sof_static_pipeline_status *status);

struct uac2_ops;
const struct uac2_ops *sof_get_uac2_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_AUDIO_PIPELINE_SOF_STATIC_PIPELINE_H__ */

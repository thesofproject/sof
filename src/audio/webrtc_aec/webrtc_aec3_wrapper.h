/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#ifndef __WEBRTC_AEC3_WRAPPER_H__
#define __WEBRTC_AEC3_WRAPPER_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct webrtc_aec3_inst webrtc_aec3_inst_t;

webrtc_aec3_inst_t* webrtc_aec3_create(int sample_rate_hz, int num_render_channels, int num_capture_channels);
int webrtc_aec3_init(webrtc_aec3_inst_t* inst, int sample_rate_hz);
int webrtc_aec3_buffer_farend(webrtc_aec3_inst_t* inst, const float* const* ref, size_t num_render_channels, size_t num_samples);
int webrtc_aec3_process(webrtc_aec3_inst_t* inst, const float* const* mic, size_t num_capture_channels,
                        float* const* out, size_t num_samples);
int webrtc_aec3_set_suppression(webrtc_aec3_inst_t* inst, bool high_suppression);
void webrtc_aec3_free(webrtc_aec3_inst_t* inst);

#ifdef __cplusplus
}
#endif

#endif /* __WEBRTC_AEC3_WRAPPER_H__ */

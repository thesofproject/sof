/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#include "webrtc_aec3_wrapper.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/aec3_buffer/audio_buffer.h"
#include <cstring>
#include <memory>

struct webrtc_aec3_inst {
    std::unique_ptr<webrtc::EchoCanceller3> aec3;
    std::unique_ptr<webrtc::AudioBuffer> render_buf;
    std::unique_ptr<webrtc::AudioBuffer> capture_buf;
    int sample_rate_hz;
    int num_channels;
};

extern "C" {

webrtc_aec3_inst_t* webrtc_aec3_create(int sample_rate_hz, int num_channels) {
    auto inst = std::make_unique<webrtc_aec3_inst>();
    inst->sample_rate_hz = sample_rate_hz;
    inst->num_channels = num_channels;

    webrtc::EchoCanceller3Config config;
    config.delay.use_external_delay_estimator = true;
    config.delay.default_delay = 5;

    config.delay.delay_headroom_samples = 0;

    // Reduce filter partition count for real-time DSP performance (1 block = 4ms tail)
    config.filter.refined.length_blocks = 1;
    config.filter.refined_initial.length_blocks = 1;
    config.filter.coarse.length_blocks = 1;
    config.filter.coarse_initial.length_blocks = 1;

    // Disable coarse filter to save 50% subtractor FFT/filter cycles
    config.filter.enable_coarse_filter_output_usage = false;

    // Disable heavy sub-band ERLE estimator by setting num_sections = 1
    config.erle.num_sections = 1;

    // Disable reverb modeling in nonlinear mode to reduce cycle count
    config.echo_model.model_reverb_in_nonlinear_mode = false;

    webrtc::EchoCanceller3Config::Validate(&config);
    inst->aec3 = std::make_unique<webrtc::EchoCanceller3>(
        config, std::nullopt, sample_rate_hz, num_channels, num_channels);
    inst->aec3->SetAudioBufferDelay(40);
    inst->aec3->SetCaptureOutputUsage(true);


    inst->render_buf = std::make_unique<webrtc::AudioBuffer>(
        sample_rate_hz, num_channels, sample_rate_hz, num_channels, sample_rate_hz, num_channels);
    inst->capture_buf = std::make_unique<webrtc::AudioBuffer>(
        sample_rate_hz, num_channels, sample_rate_hz, num_channels, sample_rate_hz, num_channels);

    return inst.release();
}

int webrtc_aec3_init(webrtc_aec3_inst_t* inst, int sample_rate_hz) {
    if (!inst || !inst->aec3) return -1;
    inst->aec3->SetAudioBufferDelay(40);
    return 0;
}

int webrtc_aec3_buffer_farend(webrtc_aec3_inst_t* inst, const float* ref, size_t num_samples) {
    if (!inst || !inst->aec3 || !ref) return -1;
    if (num_samples != 160) return -1;

    float* dst = inst->render_buf->split_bands(0)[0];
    std::memcpy(dst, ref, num_samples * sizeof(float));

    inst->aec3->AnalyzeRender(inst->render_buf.get());
    return 0;
}

int webrtc_aec3_process(webrtc_aec3_inst_t* inst, const float* const* mic, size_t num_bands,
                        float* const* out, size_t num_samples) {
    if (!inst || !inst->aec3 || !mic || !out) return -1;
    if (num_samples != 160) return -1;

    float* dst = inst->capture_buf->split_bands(0)[0];
    std::memcpy(dst, mic[0], num_samples * sizeof(float));

    inst->aec3->ProcessCapture(inst->capture_buf.get(), false);

    const float* src = inst->capture_buf->split_bands(0)[0];
    std::memcpy(out[0], src, num_samples * sizeof(float));
    return 0;
}

int webrtc_aec3_set_suppression(webrtc_aec3_inst_t* inst, bool high_suppression) {
    (void)inst;
    (void)high_suppression;
    return 0;
}

void webrtc_aec3_free(webrtc_aec3_inst_t* inst) {
    delete inst;
}

} // extern "C"

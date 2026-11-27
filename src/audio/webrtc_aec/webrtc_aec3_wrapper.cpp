/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#include "webrtc_aec3_wrapper.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/aec3_buffer/audio_buffer.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <optional>

struct webrtc_aec3_inst {
    std::unique_ptr<webrtc::EchoCanceller3> aec3;
    std::unique_ptr<webrtc::AudioBuffer> render_buf;
    std::unique_ptr<webrtc::AudioBuffer> capture_buf;
    webrtc_aec3_cfg cfg;
    std::atomic<int> pending_delay_ms{-1};
    std::atomic<bool> high_suppression{false};
};

static webrtc::EchoCanceller3Config build_aec3_config(const webrtc_aec3_cfg& cfg, bool multichannel) {
    webrtc::EchoCanceller3Config config = multichannel
        ? webrtc::EchoCanceller3::CreateDefaultMultichannelConfig()
        : webrtc::EchoCanceller3Config();

    /* Disable delay headroom when locked to a known hardware delay to eliminate slack. */
    config.delay.use_external_delay_estimator = cfg.use_external_delay_estimator;
    config.delay.default_delay = 5;
    config.delay.delay_headroom_samples = cfg.use_external_delay_estimator ? 0 : 32;

    /* Clamp adaptive filter partition count (4 ms blocks) to supported range [1..30]. */
    size_t blocks = (size_t)std::max(1, std::min(cfg.filter_length_blocks, 30));
    size_t init_blocks = std::min(blocks, (size_t)12);
    config.filter.refined.length_blocks = blocks;
    config.filter.refined_initial.length_blocks = init_blocks;
    config.filter.coarse.length_blocks = blocks;
    config.filter.coarse_initial.length_blocks = init_blocks;
    config.filter.enable_coarse_filter_output_usage = cfg.enable_coarse_filter;

    /* Keep single-section ERLE and disable nonlinear reverb modeling to fit DSP MCPS. */
    config.erle.num_sections = 1;
    config.echo_model.model_reverb_in_nonlinear_mode = false;
    config.multi_channel.detect_stereo_content = cfg.detect_stereo_content;

    if (cfg.high_suppression) {
        /* Tighten ENR transparency/suppression thresholds for noisy or nonlinear echo paths. */
        config.suppressor.normal_tuning.mask_lf.enr_transparent = 0.15f;
        config.suppressor.normal_tuning.mask_lf.enr_suppress = 0.25f;
        config.suppressor.normal_tuning.mask_hf.enr_transparent = 0.04f;
        config.suppressor.normal_tuning.mask_hf.enr_suppress = 0.07f;
        config.suppressor.normal_tuning.max_dec_factor_lf = 0.15f;
        config.suppressor.nearend_tuning.mask_lf.enr_transparent = 0.7f;
        config.suppressor.nearend_tuning.mask_lf.enr_suppress = 0.9f;
        config.suppressor.conservative_hf_suppression = true;
    }

    webrtc::EchoCanceller3Config::Validate(&config);
    return config;
}

static void create_aec3_buffers(webrtc_aec3_inst* inst) {
    inst->render_buf = std::make_unique<webrtc::AudioBuffer>(
        inst->cfg.sample_rate_hz, inst->cfg.num_render_channels,
        inst->cfg.sample_rate_hz, inst->cfg.num_render_channels,
        inst->cfg.sample_rate_hz, inst->cfg.num_render_channels);
    inst->capture_buf = std::make_unique<webrtc::AudioBuffer>(
        inst->cfg.sample_rate_hz, inst->cfg.num_capture_channels,
        inst->cfg.sample_rate_hz, inst->cfg.num_capture_channels,
        inst->cfg.sample_rate_hz, inst->cfg.num_capture_channels);
}

static void create_aec3_engine(webrtc_aec3_inst* inst) {
    /* Latch latest suppression toggle and build stereo config only when ref channels > 1. */
    inst->cfg.high_suppression = inst->high_suppression.load(std::memory_order_acquire);
    webrtc::EchoCanceller3Config config = build_aec3_config(inst->cfg, false);
    std::optional<webrtc::EchoCanceller3Config> mc_config;
    if (inst->cfg.num_render_channels > 1) {
        mc_config = build_aec3_config(inst->cfg, true);
    }

    /* Consume any queued RT delay change before recreating the EchoCanceller3 instance. */
    int delay_ms = inst->pending_delay_ms.exchange(-1, std::memory_order_acq_rel);
    if (delay_ms >= 0) {
        inst->cfg.delay_ms = delay_ms;
    }

    inst->aec3 = std::make_unique<webrtc::EchoCanceller3>(
        config, mc_config, inst->cfg.sample_rate_hz,
        inst->cfg.num_render_channels, inst->cfg.num_capture_channels);
    inst->aec3->SetAudioBufferDelay(inst->cfg.delay_ms);
    inst->aec3->SetCaptureOutputUsage(true);
}

extern "C" {

webrtc_aec3_inst_t* webrtc_aec3_create_cfg(const struct webrtc_aec3_cfg* cfg) {
    if (!cfg || (cfg->sample_rate_hz != 8000 && cfg->sample_rate_hz != 16000)) return nullptr;
    auto inst = std::make_unique<webrtc_aec3_inst>();
    inst->cfg = *cfg;
    inst->high_suppression.store(cfg->high_suppression, std::memory_order_relaxed);
    inst->pending_delay_ms.store(-1, std::memory_order_relaxed);

    create_aec3_engine(inst.get());
    create_aec3_buffers(inst.get());

    return inst.release();
}

webrtc_aec3_inst_t* webrtc_aec3_create(int sample_rate_hz, int num_render_channels, int num_capture_channels) {
    webrtc_aec3_cfg cfg = {};
    cfg.sample_rate_hz = sample_rate_hz;
    cfg.num_render_channels = num_render_channels;
    cfg.num_capture_channels = num_capture_channels;
    cfg.delay_ms = 20;
    cfg.filter_length_blocks = 12;
    cfg.use_external_delay_estimator = false;
    cfg.enable_coarse_filter = true;
    cfg.detect_stereo_content = true;
    cfg.high_suppression = false;
    return webrtc_aec3_create_cfg(&cfg);
}

int webrtc_aec3_init(webrtc_aec3_inst_t* inst, int sample_rate_hz) {
    if (!inst || !inst->aec3) return -1;
    if (sample_rate_hz != 8000 && sample_rate_hz != 16000) return -1;
    if (inst->cfg.sample_rate_hz != sample_rate_hz) {
        inst->cfg.sample_rate_hz = sample_rate_hz;
        create_aec3_buffers(inst);
    }
    create_aec3_engine(inst);
    return 0;
}

int webrtc_aec3_buffer_farend(webrtc_aec3_inst_t* inst, const float* const* ref, size_t num_render_channels, size_t num_samples) {
    if (!inst || !inst->aec3 || !inst->render_buf || !ref) return -1;
    if (num_samples != (size_t)(inst->cfg.sample_rate_hz / 100)) return -1;
    if (num_render_channels != (size_t)inst->cfg.num_render_channels) return -1;

    int pending_delay = inst->pending_delay_ms.exchange(-1, std::memory_order_acq_rel);
    if (pending_delay >= 0) {
        inst->cfg.delay_ms = pending_delay;
        inst->aec3->SetAudioBufferDelay(pending_delay);
    }

    for (size_t ch = 0; ch < num_render_channels; ++ch) {
        float* dst = inst->render_buf->split_bands(ch)[0];
        std::memcpy(dst, ref[ch], num_samples * sizeof(float));
    }

    inst->aec3->AnalyzeRender(inst->render_buf.get());
    return 0;
}

int webrtc_aec3_process(webrtc_aec3_inst_t* inst, const float* const* mic, size_t num_capture_channels,
                        float* const* out, size_t num_samples) {
    if (!inst || !inst->aec3 || !inst->capture_buf || !mic || !out) return -1;
    if (num_samples != (size_t)(inst->cfg.sample_rate_hz / 100)) return -1;
    if (num_capture_channels != (size_t)inst->cfg.num_capture_channels) return -1;

    for (size_t ch = 0; ch < num_capture_channels; ++ch) {
        float* dst = inst->capture_buf->split_bands(ch)[0];
        std::memcpy(dst, mic[ch], num_samples * sizeof(float));
    }

    inst->aec3->ProcessCapture(inst->capture_buf.get(), false);

    for (size_t ch = 0; ch < num_capture_channels; ++ch) {
        const float* src = inst->capture_buf->split_bands(ch)[0];
        std::memcpy(out[ch], src, num_samples * sizeof(float));
    }
    return 0;
}

int webrtc_aec3_set_suppression(webrtc_aec3_inst_t* inst, bool high_suppression) {
    if (!inst) return -1;
    /* Defer EchoCanceller3 reconstruction to prepare()/reset() to avoid RT allocation. */
    inst->high_suppression.store(high_suppression, std::memory_order_release);
    return 0;
}

int webrtc_aec3_set_delay(webrtc_aec3_inst_t* inst, int delay_ms) {
    if (!inst || delay_ms < 0) return -1;
    inst->pending_delay_ms.store(delay_ms, std::memory_order_release);
    return 0;
}

int webrtc_aec3_get_metrics(webrtc_aec3_inst_t* inst, int* erl_db, int* erle_db, int* delay_ms) {
    if (!inst || !inst->aec3) return -1;
    webrtc::EchoControl::Metrics m = inst->aec3->GetMetrics();
    if (erl_db) *erl_db = (int)m.echo_return_loss;
    if (erle_db) *erle_db = (int)m.echo_return_loss_enhancement;
    if (delay_ms) *delay_ms = m.delay_ms;
    return 0;
}

void webrtc_aec3_free(webrtc_aec3_inst_t* inst) {
    delete inst;
}

} // extern "C"

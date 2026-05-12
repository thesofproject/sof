/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/**
 * \file mfcc_vad.h
 * \brief Voice Activity Detection based on Mel spectrum energy.
 *
 * This VAD operates on the Q9.23 Mel log spectrum values produced by
 * the MFCC component. It tracks a per-bin noise floor that follows
 * the signal downward instantly and rises slowly, then computes a
 * speech-weighted energy delta above the floor.
 */

#ifndef __SOF_AUDIO_MFCC_MFCC_VAD_H__
#define __SOF_AUDIO_MFCC_MFCC_VAD_H__

#include <stdint.h>
#include <stdbool.h>

struct processing_module;

/**
 * \brief Number of frames for fast noise floor convergence at startup (~1 s at 10 ms/frame).
 */
#define MFCC_VAD_NOISE_INIT_FRAMES	100

/**
 * \brief Slow noise floor rise coefficient in Q1.15 (0.003 * 2^15).
 */
#define MFCC_VAD_NOISE_RISE_ALPHA	98

/**
 * \brief Fast noise floor rise coefficient in Q1.15 (0.020 * 2^15).
 */
#define MFCC_VAD_NOISE_RISE_ALPHA_FAST	655

/**
 * \brief Energy threshold for speech detection in Q9.23 (0.30 * 2^23).
 */
#define MFCC_VAD_ENERGY_THRESHOLD	2516582

/**
 * \brief Hangover frame count to keep VAD active after last speech detection.
 */
#define MFCC_VAD_HANGOVER_FRAMES	20

/**
 * \brief VAD state structure.
 */
struct mfcc_vad_state {
	int32_t *noise_floor; /**< Per-bin noise floor in Q9.23 */
	int16_t *weights; /**< Speech-frequency emphasis weights Q1.15 */
	int32_t energy; /**< Weighted signal energy in Q9.23 */
	int32_t energy_threshold; /**< Energy threshold Q9.23 */
	int32_t noise_energy; /**< Weighted noise floor energy in Q9.23 */
	int16_t frame_count; /**< Initial convergence frames processed */
	int16_t hangover_counter; /**< Current hangover counter */
	int16_t hangover_max; /**< Maximum hangover frames */
	int16_t init_frames; /**< Number of initial frames for fast convergence */
	int16_t noise_rise_alpha_fast; /**< Fast rise alpha Q1.15 */
	int16_t noise_rise_alpha_slow; /**< Slow rise alpha Q1.15 */
	int16_t num_mel_bins; /**< Number of Mel bins in use */
	bool initialized; /**< True after first frame processed */
	bool is_speech; /**< Current VAD decision */
};

/**
 * \brief Initialize VAD state.
 *
 * \param[out] vad Pointer to VAD state to initialize.
 * \param[in] num_mel_bins Number of Mel bins.
 * \param[in] sample_rate Audio sample rate in Hz.
 * \param[in] mod Processing module for memory allocation.
 * \return 0 on success, negative error code on failure.
 */
int mfcc_vad_init(struct mfcc_vad_state *vad, int num_mel_bins, int32_t sample_rate,
		  struct processing_module *mod);

/**
 * \brief Process one Mel spectrum frame and update VAD decision.
 *
 * \param[in,out] vad Pointer to VAD state.
 * \param[in] mel_log Mel log spectrum in Q9.23, array of num_mel_bins values.
 * \return 1 if speech detected, 0 if silence.
 */
int mfcc_vad_update(struct mfcc_vad_state *vad, const int32_t *mel_log);

#endif /* __SOF_AUDIO_MFCC_MFCC_VAD_H__ */

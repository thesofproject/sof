/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __USER_MFCC_H__
#define __USER_MFCC_H__

#include <stdbool.h>
#include <stdint.h>

#define MFCC_BLACKMAN_A0 Q_CONVERT_FLOAT(0.42, 15) /* For MFCC */

#define SOF_MFCC_CONFIG_MAX_SIZE	256	/* Max size for configuration data in bytes */

enum sof_mfcc_fft_pad_type {
	MFCC_PAD_END = 0,
	MFCC_PAD_CENTER = 1,
	MFCC_PAD_START = 2,
};

enum sof_mfcc_fft_window_type {
	MFCC_RECTANGULAR_WINDOW = 0,
	MFCC_BLACKMAN_WINDOW = 1,
	MFCC_HAMMING_WINDOW = 2,
	MFCC_HANN_WINDOW = 3,
	MFCC_POVEY_WINDOW = 4,
};

enum sof_mfcc_mel_log_type {
	MEL_LOG_IS_LOG = 0,
	MEL_LOG_IS_LOG10 = 1,
	MEL_LOG_IS_DB = 2
};

enum sof_mfcc_mel_norm_type {
	MFCC_MEL_NORM_NONE = 0,
	MFCC_MEL_NORM_SLANEY = 1,
};

enum sof_mfcc_dct_type {
	MFCC_DCT_I = 0,
	MFCC_DCT_II = 1,
};

/*
 * Configuration blob
 */
struct sof_mfcc_config {
	uint32_t size; /**< Size of this struct in bytes */
	uint32_t reserved[8];
	int32_t sample_frequency; /**< Hz. e.g. 16000 */
	int32_t pmin; /**< Q1.31 linear power, limit minimum Mel energy, e.g. 1e-9 */
	enum sof_mfcc_mel_log_type mel_log; /**< Use MEL_LOG_IS_LOG, LOG10 or DB*/
	enum sof_mfcc_mel_norm_type norm; /**< Use MEL_NORM_SLANEY or MEL_NORM_NONE */
	enum sof_mfcc_fft_pad_type pad; /**< Use PAD_END, PAD_CENTER, PAD_START */
	enum sof_mfcc_fft_window_type window; /**< Use RECTANGULAR_WINDOW, BLACKMAN_WINDOW, etc. */
	enum sof_mfcc_dct_type dct; /**< Must be DCT_II */
	int16_t blackman_coef; /**< Q1.15, typically set to 0.42 for BLACKMAN_WINDOW */
	int16_t cepstral_lifter; /**< Q7.9, e.g. 22.0 */
	int16_t channel; /**< -1 expect mono, 0 left, 1 right, ... */
	int16_t dither; /**< Reserved, no support */
	int16_t frame_length; /**< samples, e.g. 400 for 25 ms @ 16 kHz*/
	int16_t frame_shift; /**< samples, e.g. 160 for 10 ms @ 16 kHz */
	int16_t high_freq;  /**< Hz, set 0 for Nyquist frequency  */
	int16_t low_freq; /**< Hz, eg. 20 */
	int16_t num_ceps; /**< Number of cepstral coefficients, e.g. 13 */
	int16_t num_mel_bins; /**< Number of internal Mel bands, e.g. 23 */
	int16_t preemphasis_coefficient; /**< Q1.15, e.g. 0.97, or 0 for disable */
	int16_t top_db; /**< Q8.7 dB, limit Mel energies to this value e.g. 200 */
	int16_t vtln_high; /**< Reserved, no support */
	int16_t vtln_low; /**< Reserved, no support */
	int16_t vtln_warp; /**< Reserved, no support */
	bool htk_compat; /**< Must be false */
	bool raw_energy; /**< Reserved, no support */
	bool remove_dc_offset; /**< Reserved, no support */
	bool round_to_power_of_two; /**< Must be true (1) */
	bool snip_edges; /**< Must be true (1) */
	bool subtract_mean; /**< Must be false (0) */
	bool use_energy; /**< Must be false (0) */
	bool reserved_bool1;
	bool reserved_bool2;
	bool reserved_bool3;
} __attribute__((packed));

#endif /* __USER_MFCC_H__ */

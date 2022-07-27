/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/* Psychoacoustics related functions */

#ifndef __SOF_MATH_AUDITORY_H__
#define __SOF_MATH_AUDITORY_H__

#include <sof/math/fft.h>
#include <sof/math/log.h>
#include <stdint.h>

#define AUDITORY_EPS_Q31	1 /* Smallest nonzero Q1.31 value */
#define AUDITORY_LOG2_2P25_Q16	Q_CONVERT_FLOAT(25.0, 16) /* log2(2^25) */

enum psy_mel_log_scale {
	MEL_LOG = 0,
	MEL_LOG10,
	MEL_DB,
};

/* Struct to define Mel filterback calculation. The filterbank data is compressed into
 * a single vector from normal (half_fft_bins, mel_bins) size by storing only non-zero
 * weights values.
 *
 * Triangles data Number triangles is mel_bins
 * then for each triangle:
 *	index to next triangle 0, start fft_bin 1, length of this triangle 2,
 *	triangle weight values 3..N
 */
struct psy_mel_filterbank {
	int32_t log_mult; /**< Out, QX.Y scale for log, log10, or dB format */
	int32_t scale_log2; /**< Out, Scale as log2 Q16.16 to apply to Mel energies */
	int32_t samplerate; /**< In, Hz Q0*/
	int16_t start_freq; /**< In, Hz Q0*/
	int16_t end_freq; /**< In, Hz Q0*/
	int16_t *scratch_data1; /**< Scratch, At least half_fft_bins size */
	int16_t *scratch_data2; /**< Scratch, Packed triangles data */
	int16_t *data; /**< Out, Packed triangles data */
	int scratch_length1; /**< In, Length of first scratch */
	int scratch_length2; /**< In, Length of second scratch */
	int fft_bins; /**< In, Number of FFT bins */
	int half_fft_bins; /**< In, fft_bins / 2 + 1 */
	int mel_bins; /**< In, Number of Mel frequency bins */
	int data_length; /**< Out, Number of int16_t words in triangles data */
	enum psy_mel_log_scale mel_log_scale; /**< In, LOG, LOG10 or DB to select Mel format */
	bool slaney_normalize; /**< In, Apply Slaney type normalization for filterbank if true */
};

/**
 * \brief Convert Hz to Mel
 * \param[in]  hz  Frequency as Q16.0 Hz.
 * \return         Value as Q14.2 Mel.
 */
int16_t psy_hz_to_mel(int16_t hz);

/**
 * \brief Convert Mel to Hz
 * See Wikipedia https://en.wikipedia.org/wiki/Mel_scale
 * hz = 700 * (exp(mel / 1126.9941805389) - 1)
 *
 * \param[in]  mel  Value as Q14.2 Mel, max 4358.4 Mel.
 * \return          hz Frequency as Q16.0 Hz, max 32767 Hz.
 */
int16_t psy_mel_to_hz(int16_t mel);

/**
 * \brief Get a Mel frequency filterbank
 * See Wikipedia https://en.wikipedia.org/wiki/Mel_scale
 *
 * \param[in, out] mel_fb  Struct with filterbank input parameters, in return the band
 *			   filter coefficients.
 * \return                 Zero when success, otherwise error code.
 */
int psy_get_mel_filterbank(struct psy_mel_filterbank *mel_fb);

/**
 * \brief Convert linear complex spectra from FFT into Mel band energies in desired
 * logarithmic format.
 *
 * \param[in]  mel_fb        Struct with filterbank parameters and filter coefficients to apply.
 * \param[in]  fft_out       Array of complex numbers from FFT in Q1.15 format.
 * \param[out] power_spectra Array of linear power spectra, needed scratch are that is half + 1
 *                           side of fft_out. The data can be discarded after if no use.
 * \param[out] mel_log       Array of Q9.7 log/log10/10log10 format Mel band energies.
 * \param[in]  bitshift      A shift left scale that has been possibly applied to FFT. This will
 *                           be subtracted from the log or decibels notation.
 */
void psy_apply_mel_filterbank_16(struct psy_mel_filterbank *mel_fb, struct icomplex16 *fft_out,
				 int32_t *power_spectra, int16_t *mel_log, int bitshift);

/**
 * \brief Convert linear complex spectra from FFT into Mel band energies in desired
 * logarithmic format.
 *
 * \param[in]  mel_fb        Struct with filterbank parameters and filter coefficients to apply.
 * \param[in]  fft_out       Array of complex numbers from FFT in Q1.31 format.
 * \param[out] power_spectra Array of linear power spectra, needed scratch are that is half + 1
 *                           side of fft_out. The data can be discarded after if no use.
 * \param[out] mel_log       Array of Q9.7 log/log10/10log10 format Mel band energies.
 * \param[in]  bitshift      A shift left scale that has been possibly applied to FFT. This will
 *                           be subtracted from the log or decibels notation.
 */
void psy_apply_mel_filterbank_32(struct psy_mel_filterbank *mel_fb, struct icomplex32 *fft_out,
				 int32_t *power_spectra, int16_t *mel_log, int bitshift);

#endif /* __SOF_MATH_AUDITORY_H__ */

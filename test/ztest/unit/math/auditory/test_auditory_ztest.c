// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/math/auditory/auditory.c:
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>

#include <rtos/alloc.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/auditory.h>
#include <sof/math/icomplex16.h>
#include <sof/math/icomplex32.h>
#include <sof/math/fft.h>

#include <stdint.h>
#include <math.h>

/* Reference test vectors shared with the legacy CMocka test. They are
 * #included in place from the CMocka tree (added to the include path in
 * CMakeLists.txt) so the exact same fixed-point data is exercised.
 */
#include "ref_hz_to_mel.h"
#include "ref_mel_filterbank_16_test1.h"
#include "ref_mel_filterbank_32_test1.h"
#include "ref_mel_filterbank_16_test2.h"
#include "ref_mel_filterbank_32_test2.h"
#include "ref_mel_filterbank_16_test3.h"
#include "ref_mel_filterbank_32_test3.h"
#include "ref_mel_filterbank_16_test4.h"
#include "ref_mel_filterbank_32_test4.h"

#include "mod_alloc.h"

#define HZ_TO_MEL_MAX_ERROR_ABS 1.5f
#define HZ_TO_MEL_MAX_ERROR_RMS 0.5f
#define MEL_TO_HZ_MAX_ERROR_ABS 5.0f
#define MEL_TO_HZ_MAX_ERROR_RMS 1.5f

#define MEL_FB16_MAX_ERROR_ABS 5.0f
#define MEL_FB16_MAX_ERROR_RMS 3.0f
#define MEL_FB32_MAX_ERROR_ABS 5.0f
#define MEL_FB32_MAX_ERROR_RMS 3.0f

/* Fixed filterbank geometry of the reference vectors */
#define TEST_SAMPLERATE 16000
#define TEST_START_FREQ 100
#define TEST_END_FREQ   7500

static struct processing_module dummy;

/** @brief Running absolute and RMS error against a reference vector. */
struct error_acc {
	float sum_squares;
	float delta_max;
};

/**
 * @brief Accumulate one reference-versus-result difference.
 * @param[in,out] acc Error accumulator.
 * @param[in] delta Reference value minus computed value.
 */
static void error_acc_add(struct error_acc *acc, float delta)
{
	acc->sum_squares += delta * delta;
	if (delta > acc->delta_max) {
		acc->delta_max = delta;
	} else if (-delta > acc->delta_max) {
		acc->delta_max = -delta;
	}
}

/**
 * @brief Report the accumulated error and assert it is within the limits.
 * @param[in] acc Error accumulator.
 * @param[in] count Number of accumulated values.
 * @param[in] max_error_abs Maximum tolerated absolute error.
 * @param[in] max_error_rms Maximum tolerated RMS error.
 */
static void error_acc_check(const struct error_acc *acc, int count, float max_error_abs,
			    float max_error_rms)
{
	float error_rms = sqrtf(acc->sum_squares / (float)count);

	/* printk() is varargs, so the float arguments are promoted to double
	 * anyway; cast explicitly to keep -Wdouble-promotion quiet.
	 */
	TC_PRINT("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
		 (double)acc->delta_max, (double)max_error_abs, (double)error_rms,
		 (double)max_error_rms);

	zassert_true(error_rms < max_error_rms, "error RMS %5.2f exceeds limit %5.2f",
		     (double)error_rms, (double)max_error_rms);
	zassert_true(acc->delta_max < max_error_abs, "Max absolute error %5.2f exceeds limit %5.2f",
		     (double)acc->delta_max, (double)max_error_abs);
}

/**
 * @brief Fill in the input fields of a filterbank shared by both bit depths.
 * @param[out] fb Filterbank to initialise.
 * @param[in] num_fft_bins Number of FFT bins.
 * @param[in] num_mel_bins Number of Mel bins.
 * @param[in] norm_slaney Non-zero to apply Slaney normalization.
 * @param[in] mel_log_type Logarithmic format of the Mel energies.
 * @param[in] scratch1 First scratch buffer, at least half_fft_bins long.
 * @param[in] scratch2 Second scratch buffer, for the packed triangles.
 * @param[in] scratch_length Length of both scratch buffers, in int16_t words.
 */
static void filterbank_init(struct psy_mel_filterbank *fb, int num_fft_bins, int num_mel_bins,
			    int norm_slaney, enum psy_mel_log_scale mel_log_type, void *scratch1,
			    void *scratch2, int scratch_length)
{
	fb->samplerate = TEST_SAMPLERATE;
	fb->start_freq = TEST_START_FREQ;
	fb->end_freq = TEST_END_FREQ;
	fb->mel_bins = num_mel_bins;
	fb->slaney_normalize = (norm_slaney > 0);
	fb->mel_log_scale = mel_log_type;
	fb->fft_bins = num_fft_bins;
	fb->half_fft_bins = num_fft_bins / 2 + 1;
	fb->scratch_data1 = (int16_t *)scratch1;
	fb->scratch_data2 = (int16_t *)scratch2;
	fb->scratch_length1 = scratch_length;
	fb->scratch_length2 = scratch_length;
}

/**
 * @brief Run the 16-bit Mel filterbank and compare it to a reference vector.
 * @param[in] fft_real Real parts of the reference half spectrum, Q1.15.
 * @param[in] fft_imag Imaginary parts of the reference half spectrum, Q1.15.
 * @param[in] ref_mel_log Expected Q9.7 Mel band energies.
 * @param[in] num_fft_bins Number of FFT bins.
 * @param[in] num_mel_bins Number of Mel bins.
 * @param[in] norm_slaney Non-zero to apply Slaney normalization.
 * @param[in] mel_log_type Logarithmic format of the Mel energies.
 * @param[in] shift Left shift that was applied to the FFT.
 */
static void filterbank_16_test(const int16_t *fft_real, const int16_t *fft_imag,
			       const int16_t *ref_mel_log, int num_fft_bins, int num_mel_bins,
			       int norm_slaney, enum psy_mel_log_scale mel_log_type, int shift)
{
	struct psy_mel_filterbank fb;
	struct error_acc acc = {0};
	struct icomplex16 *fft_buf;
	struct icomplex16 *fft_out;
	int32_t *power_spectra;
	int16_t *mel_log;
	const int half_fft = num_fft_bins / 2 + 1;
	const int fft_size = num_fft_bins * sizeof(struct icomplex16);
	int i;

	fft_buf = rzalloc(SOF_MEM_FLAG_USER, fft_size);
	zassert_not_null(fft_buf, "Failed to allocate fft_buf.");

	fft_out = rzalloc(SOF_MEM_FLAG_USER, fft_size);
	zassert_not_null(fft_out, "Failed to allocate fft_out.");

	mel_log = rzalloc(SOF_MEM_FLAG_USER, num_mel_bins * sizeof(int16_t));
	zassert_not_null(mel_log, "Failed to allocate output vector.");

	filterbank_init(&fb, num_fft_bins, num_mel_bins, norm_slaney, mel_log_type, fft_buf,
			fft_out, fft_size / sizeof(int16_t));
	zassert_ok(mod_psy_get_mel_filterbank(&dummy, &fb), "Failed Mel filterbank.");

	/* Copy input from test vectors */
	for (i = 0; i < half_fft; i++) {
		fft_out[i].real = fft_real[i];
		fft_out[i].imag = fft_imag[i];
	}

	/* Run filterbank */
	power_spectra = (int32_t *)&fft_buf[0];
	psy_apply_mel_filterbank_16(&fb, fft_out, power_spectra, mel_log, shift);

	/* Check */
	for (i = 0; i < num_mel_bins; i++) {
		error_acc_add(&acc, (float)ref_mel_log[i] - (float)mel_log[i]);
	}

	mod_psy_free_mel_filterbank(&dummy, &fb);
	rfree(mel_log);
	rfree(fft_out);
	rfree(fft_buf);

	error_acc_check(&acc, num_mel_bins, MEL_FB16_MAX_ERROR_ABS, MEL_FB16_MAX_ERROR_RMS);
}

/**
 * @brief Run the 32-bit Mel filterbank and compare it to a reference vector.
 * @param[in] fft_real Real parts of the reference half spectrum, Q1.31.
 * @param[in] fft_imag Imaginary parts of the reference half spectrum, Q1.31.
 * @param[in] ref_mel_log Expected Q9.7 Mel band energies.
 * @param[in] num_fft_bins Number of FFT bins.
 * @param[in] num_mel_bins Number of Mel bins.
 * @param[in] norm_slaney Non-zero to apply Slaney normalization.
 * @param[in] mel_log_type Logarithmic format of the Mel energies.
 * @param[in] shift Left shift that was applied to the FFT.
 */
static void filterbank_32_test(const int32_t *fft_real, const int32_t *fft_imag,
			       const int16_t *ref_mel_log, int num_fft_bins, int num_mel_bins,
			       int norm_slaney, enum psy_mel_log_scale mel_log_type, int shift)
{
	struct psy_mel_filterbank fb;
	struct error_acc acc = {0};
	struct icomplex32 *fft_buf;
	struct icomplex32 *fft_out;
	int32_t *power_spectra;
	int32_t *mel_log;
	int16_t mel_log_16;
	const int half_fft = num_fft_bins / 2 + 1;
	const int fft_size = num_fft_bins * sizeof(struct icomplex32);
	int i;

	fft_buf = rzalloc(SOF_MEM_FLAG_USER, fft_size);
	zassert_not_null(fft_buf, "Failed to allocate fft_buf.");

	fft_out = rzalloc(SOF_MEM_FLAG_USER, fft_size);
	zassert_not_null(fft_out, "Failed to allocate fft_out.");

	mel_log = rzalloc(SOF_MEM_FLAG_USER, num_mel_bins * sizeof(int32_t));
	zassert_not_null(mel_log, "Failed to allocate output vector.");

	filterbank_init(&fb, num_fft_bins, num_mel_bins, norm_slaney, mel_log_type, fft_buf,
			fft_out, fft_size / sizeof(int16_t));
	zassert_ok(mod_psy_get_mel_filterbank(&dummy, &fb), "Failed Mel filterbank.");

	/* Copy input from test vectors */
	for (i = 0; i < half_fft; i++) {
		fft_out[i].real = fft_real[i];
		fft_out[i].imag = fft_imag[i];
	}

	/* Run filterbank */
	power_spectra = (int32_t *)&fft_buf[0];
	psy_apply_mel_filterbank_32(&fb, fft_out, power_spectra, mel_log, shift);

	/* Check: convert Q9.23 output to Q9.7 for comparison with reference */
	for (i = 0; i < num_mel_bins; i++) {
		mel_log_16 = (int16_t)(mel_log[i] >> 16);
		error_acc_add(&acc, (float)ref_mel_log[i] - (float)mel_log_16);
	}

	mod_psy_free_mel_filterbank(&dummy, &fb);
	rfree(mel_log);
	rfree(fft_out);
	rfree(fft_buf);

	error_acc_check(&acc, num_mel_bins, MEL_FB32_MAX_ERROR_ABS, MEL_FB32_MAX_ERROR_RMS);
}

/* Expand the per-test reference vector and geometry names of
 * ref_mel_filterbank_<bits>_test<n>.h into a filterbank_<bits>_test() call, so
 * that the 8 arguments are spelled out once instead of once per test case.
 */
#define MEL_FILTERBANK_TEST(bits, n)                                                               \
	filterbank_##bits##_test(mel_filterbank_##bits##_test##n##_real,                           \
				 mel_filterbank_##bits##_test##n##_imag,                           \
				 mel_filterbank_##bits##_test##n##_mel_log,                        \
				 MEL_FILTERBANK_##bits##_TEST##n##_FFT_SIZE,                       \
				 MEL_FILTERBANK_##bits##_TEST##n##_NUM_MEL_BINS,                   \
				 MEL_FILTERBANK_##bits##_TEST##n##_NORM_SLANEY,                    \
				 MEL_FILTERBANK_##bits##_TEST##n##_MEL_LOG,                        \
				 MEL_FILTERBANK_##bits##_TEST##n##_SHIFT)

/**
 * @brief Verify the 16-bit Mel filterbank against reference vector set 1
 */
ZTEST(math_auditory_suite, test_mel_filterbank_16_test1)
{
	MEL_FILTERBANK_TEST(16, 1);
}

/**
 * @brief Verify the 32-bit Mel filterbank against reference vector set 1
 */
ZTEST(math_auditory_suite, test_mel_filterbank_32_test1)
{
	MEL_FILTERBANK_TEST(32, 1);
}

/**
 * @brief Verify the 16-bit Mel filterbank against reference vector set 2
 */
ZTEST(math_auditory_suite, test_mel_filterbank_16_test2)
{
	MEL_FILTERBANK_TEST(16, 2);
}

/**
 * @brief Verify the 32-bit Mel filterbank against reference vector set 2
 */
ZTEST(math_auditory_suite, test_mel_filterbank_32_test2)
{
	MEL_FILTERBANK_TEST(32, 2);
}

/**
 * @brief Verify the 16-bit Mel filterbank against reference vector set 3
 */
ZTEST(math_auditory_suite, test_mel_filterbank_16_test3)
{
	MEL_FILTERBANK_TEST(16, 3);
}

/**
 * @brief Verify the 32-bit Mel filterbank against reference vector set 3
 */
ZTEST(math_auditory_suite, test_mel_filterbank_32_test3)
{
	MEL_FILTERBANK_TEST(32, 3);
}

/**
 * @brief Verify the 16-bit Mel filterbank against reference vector set 4
 */
ZTEST(math_auditory_suite, test_mel_filterbank_16_test4)
{
	MEL_FILTERBANK_TEST(16, 4);
}

/**
 * @brief Verify the 32-bit Mel filterbank against reference vector set 4
 */
ZTEST(math_auditory_suite, test_mel_filterbank_32_test4)
{
	MEL_FILTERBANK_TEST(32, 4);
}

/**
 * @brief Verify the Hz to Mel conversion against its reference vector
 */
ZTEST(math_auditory_suite, test_hz_to_mel)
{
	struct error_acc acc = {0};
	int16_t test_mel;
	int i;

	for (i = 0; i < HZ_TO_MEL_NPOINTS; i++) {
		test_mel = psy_hz_to_mel(ref_hz[i]);
		error_acc_add(&acc, (float)ref_mel[i] - (float)test_mel);
	}

	error_acc_check(&acc, HZ_TO_MEL_NPOINTS, HZ_TO_MEL_MAX_ERROR_ABS, HZ_TO_MEL_MAX_ERROR_RMS);
}

/**
 * @brief Verify the Mel to Hz conversion against its reference vector
 */
ZTEST(math_auditory_suite, test_mel_to_hz)
{
	struct error_acc acc = {0};
	int16_t test_hz;
	int i;

	for (i = 0; i < HZ_TO_MEL_NPOINTS; i++) {
		test_hz = psy_mel_to_hz(ref_mel[i]);
		error_acc_add(&acc, (float)ref_revhz[i] - (float)test_hz);
	}

	error_acc_check(&acc, HZ_TO_MEL_NPOINTS, MEL_TO_HZ_MAX_ERROR_ABS, MEL_TO_HZ_MAX_ERROR_RMS);
}

/**
 * @brief Define and initialize the auditory processing test suite
 *
 * mod_alloc_test_after() checks that each test case freed the filterbank it
 * allocated through the mod_alloc() API.
 */
ZTEST_SUITE(math_auditory_suite, NULL, NULL, NULL, mod_alloc_test_after, NULL);

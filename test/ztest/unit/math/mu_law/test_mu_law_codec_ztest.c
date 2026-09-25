// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Original tests from test/cmocka/src/math/arithmetic/mu_law_codec.c:
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>
#include <stdint.h>
#include <stdio.h>
#include <sof/math/mu_law.h>

/* Include the same fixed-point reference vectors as the CMocka test. */
#include "ref_chirp_mono_8k_s16.h"
#include "mu_law_codec.h"

/**
 * @brief Verify mu-law encoding of every chirp sample against its reference.
 *
 * Keep the legacy s16 input and 8-bit reference code words unchanged.
 */
ZTEST(math_mu_law_suite, test_mu_law_encode)
{
	uint8_t mu_law_sample, mu_law_ref;
	int i;

	for (i = 0; i < REF_DATA_SAMPLE_COUNT; i++) {
		mu_law_sample = sofm_mu_law_encode(chirp_mono_8k_s16[i]);
		mu_law_ref = ref_mulaw_enc_data[i];

		if (mu_law_sample != mu_law_ref) {
			printf("%s: difference found at %d, encoded %d, ref %d, lin %d\n", __func__,
			       i, mu_law_sample, mu_law_ref, chirp_mono_8k_s16[i]);
			zassert_true(false, "mu-law encode mismatch");
		}
	}
}

/**
 * @brief Verify mu-law decoding of every reference code word.
 *
 * Compare the expanded s16 samples with the legacy fixed-point vector.
 */
ZTEST(math_mu_law_suite, test_mu_law_decode)
{
	int16_t s16_sample, s16_ref;
	int i;

	for (i = 0; i < REF_DATA_SAMPLE_COUNT; i++) {
		s16_sample = sofm_mu_law_decode(ref_mulaw_enc_data[i]);
		s16_ref = ref_mulaw_dec_data[i];
		if (s16_sample != s16_ref) {
			printf("%s: difference found at %d, byte %d, decoded %d, ref %d\n",
			       __func__, i, ref_mulaw_enc_data[i], s16_sample, s16_ref);
			zassert_true(false, "mu-law decode mismatch");
		}
	}
}

/** @brief Register the mu-law codec tests. */
ZTEST_SUITE(math_mu_law_suite, NULL, NULL, NULL, NULL, NULL);

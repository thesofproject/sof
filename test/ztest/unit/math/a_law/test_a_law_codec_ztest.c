// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/math/arithmetic/a_law_codec.c:
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>
#include <stdint.h>
#include <stdio.h>
#include <sof/math/a_law.h>

/* Reference test vectors shared with the legacy CMocka test. They are
 * #included in place from the CMocka tree (added to the include path in
 * CMakeLists.txt) so the exact same fixed-point data is exercised.
 */
#include "ref_chirp_mono_8k_s16.h"
#include "a_law_codec.h"

/**
 * @brief Verify A-law encoding against the reference vector
 *
 * Encodes every s16 chirp sample with sofm_a_law_encode() and checks the
 * 8-bit code word matches the pre-computed reference, preserving the
 * fixed-point semantics of the original CMocka test.
 */
ZTEST(math_a_law_suite, test_a_law_encode)
{
	uint8_t a_law_sample, a_law_ref;
	int i;

	for (i = 0; i < REF_DATA_SAMPLE_COUNT; i++) {
		a_law_sample = sofm_a_law_encode(chirp_mono_8k_s16[i]);
		a_law_ref = ref_alaw_enc_data[i];

		if (a_law_sample != a_law_ref) {
			printf("%s: difference found at %d, encoded %d, ref %d, lin %d\n", __func__,
			       i, a_law_sample, a_law_ref, chirp_mono_8k_s16[i]);
			zassert_true(false, "A-law encode mismatch");
		}
	}
}

/**
 * @brief Verify A-law decoding against the reference vector
 *
 * Decodes every reference code word with sofm_a_law_decode() and checks the
 * expanded s16 sample matches the pre-computed reference, preserving the
 * fixed-point semantics of the original CMocka test.
 */
ZTEST(math_a_law_suite, test_a_law_decode)
{
	int16_t s16_sample, s16_ref;
	int i;

	for (i = 0; i < REF_DATA_SAMPLE_COUNT; i++) {
		s16_sample = sofm_a_law_decode(ref_alaw_enc_data[i]);
		s16_ref = ref_alaw_dec_data[i];
		if (s16_sample != s16_ref) {
			printf("%s: difference found at %d, decoded %d, ref %d\n", __func__, i,
			       s16_sample, s16_ref);
			zassert_true(false, "A-law decode mismatch");
		}
	}
}

/**
 * @brief Define and initialize the A-law codec test suite
 */
ZTEST_SUITE(math_a_law_suite, NULL, NULL, NULL, NULL, NULL);

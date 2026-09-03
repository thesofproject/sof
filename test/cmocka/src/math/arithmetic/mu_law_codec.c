// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <stdio.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdbool.h>
#include <cmocka.h>
#include <sof/math/mu_law.h>

#include "ref_chirp_mono_8k_s16.h"
#include "mu_law_codec.h"

static void test_mu_law_encode(void **state)
{
	(void)state;

	uint8_t mu_law_sample, mu_law_ref;
	int i;

	for (i = 0; i < REF_DATA_SAMPLE_COUNT; i++) {
		mu_law_sample = sofm_mu_law_encode(chirp_mono_8k_s16[i]);
		mu_law_ref = ref_mulaw_enc_data[i];

		if (mu_law_sample != mu_law_ref) {
			printf("%s: difference found at %d, encoded %d, ref %d, lin %d\n",
			       __func__, i, mu_law_sample, mu_law_ref, chirp_mono_8k_s16[i]);
			assert_true(false);
		}
	}
}

static void test_mu_law_decode(void **state)
{
	(void)state;

	int16_t s16_sample, s16_ref;
	int i;

	for (i = 0; i < REF_DATA_SAMPLE_COUNT; i++) {
		s16_sample = sofm_mu_law_decode(ref_mulaw_enc_data[i]);
		s16_ref = ref_mulaw_dec_data[i];
		if (s16_sample != s16_ref) {
			printf("%s: difference found at %d, byte %d, decoded %d, ref %d\n",
			       __func__, i, ref_mulaw_enc_data[i], s16_sample, s16_ref);
			assert_true(false);
		}
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_mu_law_encode),
		cmocka_unit_test(test_mu_law_decode),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}

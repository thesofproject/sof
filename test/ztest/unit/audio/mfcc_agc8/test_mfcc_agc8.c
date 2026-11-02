// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>
#include <sof/audio/mfcc/mfcc_agc8.h>
#include <user/mfcc.h>
#include <string.h>

#define NBINS 40
#define ONE_Q23 (1 << 23)

static struct mfcc_agc8 agc;
static int32_t mel[NBINS];
static int8_t out[NBINS];

static void fill(int32_t value, int32_t peak)
{
	int i;

	for (i = 0; i < NBINS; i++)
		mel[i] = value;
	mel[NBINS / 2] = peak;
}

static void agc_default_init(void)
{
	struct sof_mfcc_config config;

	memset(&config, 0, sizeof(config));
	mfcc_agc8_init(&agc, &config);
}

ZTEST(mfcc_agc8_suite, test_init_defaults)
{
	agc_default_init();
	zassert_equal(agc.target, MFCC_AGC8_TARGET_Q23);
	zassert_equal(agc.floor, MFCC_AGC8_FLOOR_Q23);
	zassert_equal(agc.release, MFCC_AGC8_RELEASE_Q23);
	zassert_equal(agc.release_speech, MFCC_AGC8_RELEASE_SPEECH_Q23);
	zassert_equal(agc.gain, MFCC_AGC8_TARGET_Q23);
}

ZTEST(mfcc_agc8_suite, test_init_blob)
{
	struct sof_mfcc_config config;

	memset(&config, 0, sizeof(config));
	config.agc_target = 64;		/* 0.5 in Q8.7 */
	config.agc_floor = -128;	/* -1.0 in Q8.7 */
	config.agc_release = 1000;
	config.agc_release_speech = 100;
	mfcc_agc8_init(&agc, &config);
	zassert_equal(agc.target, ONE_Q23 / 2);
	zassert_equal(agc.floor, -ONE_Q23);
	zassert_equal(agc.release, 1000);
	zassert_equal(agc.release_speech, 100);
	zassert_equal(agc.gain, ONE_Q23 / 2);
}

ZTEST(mfcc_agc8_suite, test_quantize_no_attack)
{
	/* Peak 0.5 + gain 0.25 stays below +1.0, no attack */
	agc_default_init();
	fill(0, ONE_Q23 / 2);
	mfcc_agc8_process(&agc, mel, out, NBINS, false);
	zassert_equal(agc.gain, MFCC_AGC8_TARGET_Q23);
	zassert_equal(out[0], 32, "0.25 should be 32 in Q1.7, got %d", out[0]);
	zassert_equal(out[NBINS / 2], 96, "0.75 should be 96 in Q1.7, got %d", out[NBINS / 2]);
}

ZTEST(mfcc_agc8_suite, test_attack_and_saturation)
{
	/* Peak 3.0 limits gain to -2.0, peak maps to +1.0 and saturates to 127 */
	agc_default_init();
	fill(-2 * ONE_Q23, 3 * ONE_Q23);
	mfcc_agc8_process(&agc, mel, out, NBINS, false);
	zassert_equal(out[NBINS / 2], 127);
	zassert_equal(out[0], -128, "-4.0 should clamp to -128, got %d", out[0]);
	zassert_equal(agc.gain, -2 * ONE_Q23 + MFCC_AGC8_RELEASE_Q23);
}

ZTEST(mfcc_agc8_suite, test_floor)
{
	/* Peak 5.0 would need gain -4.0, floor keeps it at -2.0 */
	agc_default_init();
	fill(0, 5 * ONE_Q23);
	mfcc_agc8_process(&agc, mel, out, NBINS, true);
	zassert_equal(out[NBINS / 2], 127);
	zassert_equal(out[0], -128);
	zassert_equal(agc.gain, MFCC_AGC8_FLOOR_Q23 + MFCC_AGC8_RELEASE_SPEECH_Q23);
}

ZTEST(mfcc_agc8_suite, test_release_rates)
{
	int32_t gain;

	agc_default_init();
	fill(0, ONE_Q23);
	mfcc_agc8_process(&agc, mel, out, NBINS, false);
	gain = agc.gain;
	zassert_equal(gain, MFCC_AGC8_RELEASE_Q23);

	/* Quiet hops: speech releases with the slow step, silence with the normal step */
	fill(-ONE_Q23, -ONE_Q23);
	mfcc_agc8_process(&agc, mel, out, NBINS, true);
	zassert_equal(agc.gain, gain + MFCC_AGC8_RELEASE_SPEECH_Q23);
	gain = agc.gain;
	mfcc_agc8_process(&agc, mel, out, NBINS, false);
	zassert_equal(agc.gain, gain + MFCC_AGC8_RELEASE_Q23);
}

ZTEST(mfcc_agc8_suite, test_release_stops_at_target)
{
	agc_default_init();
	agc.gain = MFCC_AGC8_TARGET_Q23 - 1;
	fill(-ONE_Q23, -ONE_Q23);
	mfcc_agc8_process(&agc, mel, out, NBINS, false);
	zassert_equal(agc.gain, MFCC_AGC8_TARGET_Q23);
}

ZTEST(mfcc_agc8_suite, test_rounding)
{
	/* Gain 0.25: -0.25 + 1/256 rounds to -31 in Q1.7, -0.25 - 1/256 to -32 */
	agc_default_init();
	fill(-ONE_Q23 / 2 + (ONE_Q23 >> 8), 0);
	mel[1] = -ONE_Q23 / 2 - (ONE_Q23 >> 8);
	mfcc_agc8_process(&agc, mel, out, NBINS, false);
	zassert_equal(out[0], -31, "got %d", out[0]);
	zassert_equal(out[1], -32, "got %d", out[1]);
}

ZTEST_SUITE(mfcc_agc8_suite, NULL, NULL, NULL, NULL, NULL);

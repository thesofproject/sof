// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

ZTEST(audio_mic_privacy_manager, test_mic_privacy_manager_init)
{
#ifndef CONFIG_INTEL_ADSP_MIC_PRIVACY
	ztest_test_skip();
#endif
}

ZTEST_SUITE(audio_mic_privacy_manager, NULL, NULL, NULL, NULL, NULL);

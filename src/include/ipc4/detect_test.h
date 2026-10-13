/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

/**
 * \file include/ipc4/detect_test.h
 * \brief Keyword Detector (KD) test module configuration and parameter definitions.
 * \ingroup ipc4_telemetry_probe_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_DETECT_TEST_H__
#define __SOF_IPC4_DETECT_TEST_H__

/**
 * \brief Parameter IDs for Keyword Detector test module LargeConfigSet/Get.
 * \ingroup ipc4_telemetry_probe_api
 */
enum ipc4_detect_test_module_config_params {
	/** Send neural network keyword detection model binary blob via LargeConfigSet. */
	IPC4_DETECT_TEST_SET_MODEL_BLOB = 1,

	/** Configure Keyword Detector operational thresholds and parameters via LargeConfigSet. */
	IPC4_DETECT_TEST_SET_CONFIG = 2,

	/** Query current Keyword Detector operational configuration via LargeConfigGet. */
	IPC4_DETECT_TEST_GET_CONFIG = 3
};

#endif

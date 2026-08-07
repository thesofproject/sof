/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

/**
 * \file include/ipc4/detect_test.h
 * \brief KD module definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_DETECT_TEST_H__
#define __SOF_IPC4_DETECT_TEST_H__

enum ipc4_detect_test_module_config_params {
	/* Use LARGE_CONFIG_SET to process model blob. Ipc mailbox must
	 * contain properly configured BLOB for Detect Keyword module.
	 */
	IPC4_DETECT_TEST_SET_MODEL_BLOB = 1,

	/* Use LARGE_CONFIG_SET to set Detect Keyword module parameters.
	 * Ipc mailbox must contain properly built sof_detect_test_config
	 * struct.
	 */
	IPC4_DETECT_TEST_SET_CONFIG = 2,

	/* Use LARGE_CONFIG_GET to retrieve Detect Test config
	 * Ipc mailbox must contain properly built sof_detect_test_config
	 * struct.
	 */
	IPC4_DETECT_TEST_GET_CONFIG = 3,

	/*
	 * Use LARGE_CONFIG_SET to assign this detector's slot index in the
	 * WOV arbiter (0, 1, or 2).  Payload is a single uint8_t.
	 * Set to WOV_SLOT_INVALID (0xff) to disable arbiter integration.
	 */
	IPC4_DETECT_TEST_SET_WOV_SLOT = 4
};
#endif

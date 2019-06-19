/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_USER_DETECT_TEST_H__
#define __INCLUDE_UAPI_USER_DETECT_TEST_H__

/** IPC blob types */
#define SOF_DETECT_TEST_CONFIG	0
#define SOF_DETECT_TEST_MODEL	1

struct sof_detect_test_config {
	uint32_t size;

	/** synthetic system load settings */
	uint32_t load_mips;
	uint32_t load_memory_size;
	/** time in ms after which detection is activated */
	uint32_t preamble_time;

	/** activation right shift, determines the speed of activation */
	uint16_t activation_shift;

	/** activation threshold */
	int16_t activation_threshold;

	/** default draining size in bytes */
	uint32_t history_depth;

	/** reserved for future use */
	uint32_t reserved[2];
} __attribute__((packed));

/** used for binary blob size sanity checks */
#define SOF_DETECT_TEST_MAX_CFG_SIZE sizeof(struct sof_detect_test_config)

#endif

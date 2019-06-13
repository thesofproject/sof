/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_USER_DETECT_TEST_H__
#define __INCLUDE_UAPI_USER_DETECT_TEST_H__

struct sof_detect_test_config {
	uint32_t size; /**< size of the entire structure including data */

	/** synthetic system load settings */
	uint32_t load_mips;

	/** size of the data */
	uint32_t model_size;

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

	/* detection model (serves no purpose in the testing component) */
	uint32_t model[0];
} __attribute__((packed));

#endif
